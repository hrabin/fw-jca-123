#!/usr/bin/env python3
"""Low-level firmware update over the debug UART (USART1).

Replaces flash.sh. Standard library only (no pyserial): the serial port is
driven directly through termios/select.

Flow
----
  1. build the container image            (make deploy)
  2. open the serial port                 (115200 8N1, raw)
  3. authenticate + reboot into the bootloader
         AUTH="<password>"  -> DONE | ERROR
         REBOOT=1           -> "reboot to BL"
     skipped with -b/--bootloader-mode, or automatically when the unit is
     already sitting in the bootloader (detected with VER, which the
     bootloader ignores)
  4. stream the Intel-HEX container       one line per record -> OK | ERROR
  5. trigger the flash and check the result  "F"              -> FLASH SUCCESSFUL

Compared with the shell version every step is now checked:
  * the password is verified up front, so a wrong password fails immediately
    instead of turning into a confusing failure later;
  * responses are read until their terminating newline / marker, with a
    timeout, instead of one blocking read compared on the first two
    characters;
  * an ERROR on any HEX record aborts and reports the record number;
  * the flash is triggered and its outcome (FLASH SUCCESSFUL / FLASH FAILED /
    APP CHSUM FAILED) is verified instead of assumed.

Note: REBOOT=1 answers ERROR on success.  Its handler returns false on
purpose (app/cmd.c: _cmd_reboot_set) because the device is about to reset, so
that ERROR is expected here and is not treated as a failure.
"""

import argparse
import glob
import os
import select
import subprocess
import sys
import termios
import time

DEFAULT_DEVICE = "/dev/ttyUSB0"
DEFAULT_PASSWORD = "uhlokRopnude"   # cfg_table.c default SYSTEM password
BAUDRATE = termios.B115200

# Response vocabulary:
#   app        : app/text.c            OK / ERROR / DONE
#   bootloader : bootloader/main.c     OK / ERROR per HEX line, FLASH / TIMEOUT REBOOT
#   bootloader : bootloader/main.c     FLASH REQUEST / FLASH SUCCESSFUL / FLASH FAILED
#   both       : FATAL ERROR / APP CHSUM FAILED / BL START / BOOT STAY / BL RUN APP
T_DONE = "DONE"
T_ERROR = "ERROR"
T_OK = "OK"
T_REBOOT_BL = "reboot to BL"
T_BOOT_READY = "BOOT STAY"
T_BL_START = "BL START"
T_FLASH = "FLASH"
T_FLASH_OK = "FLASH SUCCESSFUL"
T_BL_RUN = "BL RUN APP"
T_FLASH_FAIL = "FLASH FAILED"
T_CHSUM_FAIL = "APP CHSUM FAILED"
T_FATAL = "FATAL ERROR"
T_APP_START = "APP START"

# Tokens that mean the flash attempt ended badly; checked before success so an
# "APP CHSUM FAILED" banner cannot be mistaken for a good result.
FAIL_TOKENS = [T_FLASH_FAIL, T_CHSUM_FAIL, T_FATAL, "NO FIRMWARE AVAILABLE"]


def log(msg):
    print(msg, flush=True)


def die(msg):
    print("error: %s" % msg, file=sys.stderr, flush=True)
    sys.exit(1)


class Serial:
    """Raw 8N1 serial port with timeout-based reads."""

    def __init__(self, device, verbose=False):
        self.device = device
        self.verbose = verbose
        self.rx = bytearray()
        try:
            self.fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError as e:
            die("cannot open %s: %s" % (device, e.strerror))
        try:
            self._configure()
        except termios.error as e:
            os.close(self.fd)
            die("cannot configure %s: %s" % (device, e))

    def _configure(self):
        a = termios.tcgetattr(self.fd)
        # raw, no echo, no flow control, no CR/NL translation
        a[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK |
                  termios.ISTRIP | termios.INLCR | termios.IGNCR |
                  termios.ICRNL | termios.IXON | termios.IXOFF | termios.IXANY)
        a[1] &= ~termios.OPOST
        a[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB |
                  termios.CRTSCTS)
        a[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
        a[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
                  termios.ISIG | termios.IEXTEN)
        a[4] = a[5] = BAUDRATE
        a[6][termios.VMIN] = 0
        a[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def close(self):
        os.close(self.fd)

    def flush(self):
        termios.tcflush(self.fd, termios.TCIFLUSH)
        del self.rx[:]

    def send(self, text):
        if self.verbose:
            log("  -> %s" % text)
        os.write(self.fd, (text + "\n").encode())

    def _pump(self, timeout):
        r, _, _ = select.select([self.fd], [], [], max(0.0, timeout))
        if not r:
            return False
        try:
            chunk = os.read(self.fd, 4096)
        except (BlockingIOError, OSError):
            return False
        if not chunk:
            return False
        self.rx += chunk
        if self.verbose:
            sys.stdout.write(chunk.decode(errors="replace"))
            sys.stdout.flush()
        return True

    def read_until(self, tokens, timeout):
        """Read until any token appears.  Returns (text, token|None).

        Consumes everything received, including the matched token.
        """
        deadline = time.monotonic() + timeout
        while True:
            for t in tokens:
                if self.rx.find(t.encode()) >= 0:
                    text = self.rx.decode(errors="replace")
                    del self.rx[:]
                    return text, t
            if time.monotonic() >= deadline:
                text = self.rx.decode(errors="replace")
                del self.rx[:]
                return text, None
            self._pump(min(0.25, max(0.0, deadline - time.monotonic())))

    def read_line(self, timeout):
        """Read the next non-empty line.  Returns None on timeout."""
        deadline = time.monotonic() + timeout
        while True:
            i = self.rx.find(b"\n")
            if i >= 0:
                line = bytes(self.rx[:i]).decode(errors="replace").strip("\r \t")
                del self.rx[:i + 1]
                if line:
                    if self.verbose:
                        log("  <- %s" % line)
                    return line
                continue
            if time.monotonic() >= deadline:
                return None
            self._pump(min(0.25, max(0.0, deadline - time.monotonic())))

    def drain(self, quiet=0.4, limit=10.0):
        """Discard output until the line has been quiet for `quiet` seconds.

        Used to swallow the bootloader banner (BL START ... BOOT STAY ...
        INIT FLASH device ...) so it cannot be mistaken for the reply to the
        first record we send.
        """
        deadline = time.monotonic() + limit
        last = time.monotonic()
        seen = bytearray()
        while time.monotonic() < deadline:
            if self._pump(0.05):
                last = time.monotonic()
            elif time.monotonic() - last >= quiet:
                break
        seen += self.rx
        del self.rx[:]
        return seen.decode(errors="replace")

    def command(self, text, timeout):
        """Send an app command and wait for its DONE / ERROR terminator.

        Returns the reply text; raises on timeout.
        """
        self.flush()
        self.send(text)
        reply, token = self.read_until([T_DONE, T_ERROR], timeout)
        if token is None:
            raise FlashError(
                "%s: no reply within %gs (got %r)" % (text, timeout, reply.strip()))
        return reply, token


class FlashError(Exception):
    pass


def build_image(app_dir, timeout):
    cmd = ["make", "deploy"]
    log("[1/5] building container image (make deploy)")
    try:
        r = subprocess.run(cmd, cwd=app_dir, timeout=timeout)
    except FileNotFoundError:
        die("make not found")
    except subprocess.TimeoutExpired:
        die("build timed out after %gs" % timeout)
    if r.returncode != 0:
        die("build failed (make deploy -> %d)" % r.returncode)


def find_container(build_dir, explicit):
    if explicit:
        if not os.path.isfile(explicit):
            die("container file not found: %s" % explicit)
        return explicit
    names = sorted(glob.glob(os.path.join(build_dir, "*_container.hex")))
    if not names:
        die("no *_container.hex in %s - run 'make deploy' in app/" % build_dir)
    return names[-1]


def step_authenticate(ser, password, timeout):
    log("[3/5] authenticating")
    if '"' in password:
        die('password must not contain a double quote')
    reply, token = ser.command('AUTH="%s"' % password, timeout)
    if token != T_DONE:
        # The read-only AUTH form reports the level the device ended up at,
        # which is the useful hint when a password does not match.
        level = ""
        try:
            r2, _ = ser.command("AUTH", timeout)
            for line in r2.splitlines():
                if line.strip().startswith("AUTH:"):
                    level = line.strip()
        except FlashError:
            pass
        raise FlashError(
            "authentication failed - check --password\n"
            "        device said: %s%s"
            % (reply.strip() or token, ("\n        " + level) if level else ""))


def step_reboot_to_bootloader(ser, timeout):
    """Ask the app to reboot and stay in the bootloader."""
    log("[3/5] rebooting into bootloader")
    ser.flush()
    ser.send("REBOOT=1")
    # Success marker is the app's own message; the trailing ERROR is expected
    # because _cmd_reboot_set() returns false while the device resets.
    reply, token = ser.read_until([T_REBOOT_BL, T_ERROR], timeout)
    if token != T_REBOOT_BL:
        raise FlashError(
            "REBOOT=1 was rejected - is the firmware running and authorised?\n"
            "        device said: %s" % (reply.strip() or "no reply"))

    # Now wait for the bootloader banner and for it to settle in its
    # command loop before streaming anything at it.
    deadline = time.monotonic() + timeout
    seen = ""
    while time.monotonic() < deadline:
        text, token = ser.read_until([T_BOOT_READY, T_FATAL], deadline - time.monotonic())
        seen += text
        if token == T_FATAL:
            raise FlashError("bootloader reported: %s" % text.strip())
        if token == T_BOOT_READY:
            return
    raise FlashError(
        "device did not enter the bootloader\n"
        "        last output: %s" % (seen.strip()[-200:] or "nothing"))


def _record_reply(ser, timeout):
    """Wait for a per-record reply.  Returns 'OK', 'ERROR' or None.

    The bootloader answers every HEX record with exactly OK or ERROR, but
    banner text can still be in flight when the first record goes out.  Such
    stray lines are reported and skipped - a record is only ever *accepted* on
    an explicit OK and only ever *rejected* on an explicit ERROR.
    """
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return None
        line = ser.read_line(remaining)
        if line is None:
            return None
        if line in (T_OK, T_ERROR):
            return line
        log("      (device: %s)" % line)


def step_upload(ser, container, timeout):
    with open(container, "r") as fh:
        # Strip blank lines and the trailing newline; keep every HEX record
        # including the ":00000001FF" EOF record.
        records = [ln.strip() for ln in fh if ln.strip()]

    log("[4/5] uploading %d records from %s" % (len(records), os.path.basename(container)))
    ser.flush()

    for n, rec in enumerate(records, 1):
        ser.send(rec)
        line = _record_reply(ser, timeout)
        if line is None:
            raise FlashError(
                "record %d/%d: no OK/ERROR within %gs\n        sent: %s"
                % (n, len(records), timeout, rec))
        if line != T_OK:
            raise FlashError(
                "record %d/%d rejected by the device\n"
                "        sent: %s\n"
                "        (the bootloader rejects a malformed HEX record with "
                "ERROR; a running application answers ERROR too - use -b once "
                "the bootloader is up)"
                % (n, len(records), rec))
        if n % 500 == 0:
            log("      %d/%d" % (n, len(records)))


def step_flash(ser, timeout):
    """Send 'F' and verify the bootloader actually programmed and started it.

    FLASH SUCCESSFUL only means the image was copied out of external storage.
    We keep going and require BL RUN APP (the bootloader's own CRC check
    passed) and then the application's own startup banner, so a bad image is
    reported as a failure instead of a success.
    """
    log("[5/5] triggering flash")
    ser.flush()
    ser.send("F")

    deadline = time.monotonic() + timeout
    seen = ""
    while time.monotonic() < deadline:
        text, token = ser.read_until(
            [T_FLASH_OK, T_BL_RUN, T_APP_START] + FAIL_TOKENS,
            deadline - time.monotonic())
        seen += text
        if token in FAIL_TOKENS:
            raise FlashError(
                "bootloader reported '%s'\n        output: %s"
                % (token, seen.strip()[-300:]))
        if token == T_FLASH_OK:
            log("      bootloader: FLASH SUCCESSFUL (image copied)")
            continue
        if token == T_BL_RUN:
            log("      bootloader: BL RUN APP (CRC verified, starting app)")
            continue
        if token == T_APP_START:
            # read_until() consumes whatever arrived in the same chunk, so the
            # app banner may already be inside `seen` - search both.
            banner = seen + ser.drain(quiet=0.4, limit=5.0)
            for line in banner.splitlines():
                line = line.strip()
                if line.startswith(("# CRC:", "# SN:", "# APP BUILD DATE:", "# DIAG")):
                    log("      %s" % line)
            return True
    raise FlashError(
        "the application did not start within %gs\n        last output: %s"
        % (timeout, seen.strip()[-300:] or "nothing"))


def probe(ser, timeout):
    """Non-destructive check: VER needs no authorisation."""
    log("probing %s (VER)" % ser.device)
    reply, token = ser.command("VER", timeout)
    log("device replied:\n%s" % reply.strip())
    if token != T_DONE:
        die("device did not answer VER - wrong port or firmware not running?")
    log("link OK")


def detect_app(ser, timeout):
    """True if the application is running.

    Every app command answers with DONE/ERROR, so VER is a reliable probe.
    The bootloader ignores lines it does not recognise and stays silent, which
    also covers the case where a previous flash left the unit in the
    bootloader - we can then simply resume instead of trying to authorise and
    reboot a device that has no application running.
    """
    ser.flush()
    ser.send("VER")
    _text, token = ser.read_until([T_DONE, T_ERROR], timeout)
    return token == T_DONE


def main():
    ap = argparse.ArgumentParser(
        description="Flash the JCA-123 firmware over the serial bootloader.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="defaults: --device %s  --password %s"
               % (DEFAULT_DEVICE, DEFAULT_PASSWORD))
    ap.add_argument("-d", "--device", default=DEFAULT_DEVICE,
                    help="serial device (default: %s)" % DEFAULT_DEVICE)
    ap.add_argument("-p", "--password", default=DEFAULT_PASSWORD,
                    help="SYSTEM password for AUTH (default: the built-in one)")
    ap.add_argument("-f", "--hex", metavar="FILE",
                    help="container HEX to upload (default: newest app/build/*_container.hex)")
    ap.add_argument("--no-build", action="store_true",
                    help="skip 'make deploy' and use the existing container")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--probe", action="store_true",
                      help="only check the link (VER) and exit - nothing is flashed")
    mode.add_argument("-b", "--bootloader-mode", action="store_true",
                      help="assume the bootloader is already running and skip "
                           "AUTH/REBOOT")
    ap.add_argument("-t", "--timeout", type=float, default=600.0, metavar="SEC",
                    help="overall timeout budget in seconds (default: 600)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="print every byte sent and received")
    args = ap.parse_args()

    root = os.path.dirname(os.path.abspath(__file__))
    app_dir = os.path.join(root, "app")
    if not os.path.isdir(app_dir):
        die("cannot find app/ next to %s" % os.path.basename(__file__))

    # per-step timeouts, scaled from the overall budget
    t_cmd = min(30.0, max(5.0, args.timeout / 20.0))     # one command round trip
    t_boot = min(60.0, max(10.0, args.timeout / 10.0))   # reboot -> bootloader
    t_rec = min(10.0, max(2.0, args.timeout / 100.0))    # one HEX record
    t_flash = args.timeout / 2.0                          # program + verify

    build_dir = os.path.join(app_dir, "build")
    if args.probe or args.no_build:
        log("[1/5] build skipped (%s)"
            % ("--probe" if args.probe else "--no-build"))
    else:
        build_image(app_dir, args.timeout)

    log("[2/5] opening %s @ 115200 8N1" % args.device)
    ser = Serial(args.device, verbose=args.verbose)

    try:
        if args.probe:
            probe(ser, t_cmd)
            return 0

        container = find_container(build_dir, args.hex)

        if args.bootloader_mode:
            # -b: the caller states the bootloader is already up, so the
            # application is neither running nor reachable and both AUTH and
            # REBOOT would be ignored (or, worse, answered by the app).
            log("[3/5] bootloader mode (-b) - skipping auth/reboot")
        elif detect_app(ser, t_cmd):
            step_authenticate(ser, args.password, t_cmd)
            step_reboot_to_bootloader(ser, t_boot)
        else:
            log("[3/5] device is already in the bootloader - skipping auth/reboot")

        # Swallow the banner (BL START ... BOOT STAY ... INIT FLASH device ...)
        # so it cannot be read back as the reply to the first record.
        banner = ser.drain()
        if args.verbose and banner.strip():
            log("      banner: %s" % banner.strip().replace("\n", " | "))

        step_upload(ser, container, t_rec)
        step_flash(ser, t_flash)

        log("")
        log("flashed successfully - new firmware is running")
        return 0
    except FlashError as e:
        print("error: %s" % e, file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr, flush=True)
        return 130
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
