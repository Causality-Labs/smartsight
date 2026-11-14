import os
import sys
import time
import traceback


class _SubTest:
    def __init__(self, case, params):
        self.case = case
        self.label = ", ".join(f"{k}={v!r}" for k, v in params.items())

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        if exc_type is None:
            return False

        if issubclass(exc_type, AssertionError):
            self.case.sub_failures.append(("FAIL", f"[{self.label}] {exc}"))
            return True

        if issubclass(exc_type, Exception):
            text = "".join(traceback.format_exception(exc_type, exc, tb))
            self.case.sub_failures.append(("ERROR", f"[{self.label}]\n{text}"))
            return True

        return False


class TestCase:
    skip_reason = None

    def __init__(self, method_name):
        self.method_name = method_name
        self._cleanups = []
        self.sub_failures = []

    @classmethod
    def setUpClass(cls):
        pass

    def setUp(self):
        pass

    def addCleanup(self, func, *args):
        self._cleanups.append((func, args))

    def subTest(self, **params):
        return _SubTest(self, params)

    def fail(self, msg=None):
        raise AssertionError(msg or "test failed")

    def _check(self, ok, default, msg):
        if not ok:
            raise AssertionError(f"{msg}: {default}" if msg else default)

    def assertTrue(self, value, msg=None):
        self._check(bool(value), f"{value!r} is not true", msg)

    def assertFalse(self, value, msg=None):
        self._check(not value, f"{value!r} is not false", msg)

    def assertEqual(self, a, b, msg=None):
        self._check(a == b, f"{a!r} != {b!r}", msg)

    def assertGreater(self, a, b, msg=None):
        self._check(a > b, f"{a!r} is not greater than {b!r}", msg)

    def assertGreaterEqual(self, a, b, msg=None):
        self._check(a >= b, f"{a!r} is not greater than or equal to {b!r}", msg)

    def assertLess(self, a, b, msg=None):
        self._check(a < b, f"{a!r} is not less than {b!r}", msg)

    def assertLessEqual(self, a, b, msg=None):
        self._check(a <= b, f"{a!r} is not less than or equal to {b!r}", msg)

    def assertAlmostEqual(self, a, b, delta, msg=None):
        self._check(abs(a - b) <= delta, f"{a!r} and {b!r} differ by more than {delta!r}", msg)


def discover(tests_dir):
    sys.path.insert(0, tests_dir)
    classes = []

    for filename in sorted(os.listdir(tests_dir)):
        if not (filename.startswith("test_") and filename.endswith(".py")):
            continue

        modname = filename[:-3]
        module = __import__(modname)

        for attr in sorted(dir(module)):
            obj = getattr(module, attr)
            if isinstance(obj, type) and issubclass(obj, TestCase) and obj.__module__ == modname:
                classes.append(obj)

    return classes


def _test_names(cls):
    return sorted(n for n in dir(cls) if n.startswith("test") and callable(getattr(cls, n)))


def _run_case(case):
    problems = []

    try:
        case.setUp()
        getattr(case, case.method_name)()
    except AssertionError as e:
        problems.append(("FAIL", str(e) or "assertion failed"))
    except Exception:
        problems.append(("ERROR", traceback.format_exc()))

    problems.extend(case.sub_failures)

    for func, args in reversed(case._cleanups):
        try:
            func(*args)
        except AssertionError as e:
            problems.append(("FAIL", f"in cleanup: {e}"))
        except Exception:
            problems.append(("ERROR", "in cleanup:\n" + traceback.format_exc()))

    return problems


def run(classes, patterns=(), failfast=False):
    ran = 0
    failures = 0
    errors = 0
    skipped = 0
    report = []
    stop = False
    started = time.time()

    try:
        for cls in classes:
            if stop:
                break

            names = []
            for name in _test_names(cls):
                test_id = f"{cls.__module__}.{cls.__name__}.{name}"
                if not patterns or any(p in test_id for p in patterns):
                    names.append(name)

            if not names:
                continue

            if cls.skip_reason:
                for name in names:
                    print(f"{cls.__module__}.{cls.__name__}.{name} ... skipped ({cls.skip_reason})")
                    skipped += 1
                continue

            try:
                cls.setUpClass()
            except Exception:
                print(f"{cls.__module__}.{cls.__name__}.setUpClass ... ERROR")
                errors += 1
                report.append((f"{cls.__name__}.setUpClass", [("ERROR", traceback.format_exc())]))
                stop = failfast
                continue

            for name in names:
                test_id = f"{cls.__module__}.{cls.__name__}.{name}"
                print(f"{test_id} ... ", end="", flush=True)

                problems = _run_case(cls(name))
                ran += 1

                if not problems:
                    print("ok")
                    continue

                if any(kind == "ERROR" for kind, _ in problems):
                    print("ERROR")
                    errors += 1
                else:
                    print("FAIL")
                    failures += 1

                report.append((test_id, problems))

                if failfast:
                    stop = True
                    break
    except KeyboardInterrupt:
        print("\ninterrupted")

    for test_id, problems in report:
        for kind, text in problems:
            print("=" * 70)
            print(f"{kind}: {test_id}")
            print("-" * 70)
            print(text)

    print("-" * 70)
    print(f"Ran {ran} tests in {time.time() - started:.3f}s")

    extras = f", skipped={skipped}" if skipped else ""
    if failures or errors:
        print(f"FAILED (failures={failures}, errors={errors}{extras})")
        return False

    print(f"OK{f' (skipped={skipped})' if skipped else ''}")
    return True
