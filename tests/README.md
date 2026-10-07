# Authentication validation

Host credential parser:

```sh
/usr/bin/clang++ -std=c++17 \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" -Isrc \
  tests/credentials_test.cpp src/telegram/credentials.cpp \
  -o /private/tmp/vita-tg-credentials-test
/private/tmp/vita-tg-credentials-test /private/tmp/vita-tg-test.conf
```

Run from the application root. The argument names a disposable test fixture,
never the real credentials file. The parser test overwrites and removes it.

Vita3K tests and limitations are in [authentication](../docs/AUTHENTICATION.md).
`vita-port` checks platform primitives, including concurrent thread-local
isolation. `vita-auth` uses the actual application Auth class, fictitious app
credentials and a separate directory. It checks reaching WaitPhoneNumber,
reopening the encrypted binlog and joining client workers on shutdown. It does
not send phone numbers or verification codes or prove server authorization.

Manual account validation, after supplying our application credentials:

1. Confirm that missing/invalid configuration stays not signed in.
2. Use disposable test-DC credentials/accounts first. Enter phone and the
   server-directed code through the native IME; verify rejected codes remain
   editable and do not mark the account signed in.
3. Verify email/code and masked 2FA states when requested by the server.
4. Verify Ready, restart/session restoration, logout/session invalidation and
   server-directed resend/flood waits.
5. Repeat on real hardware; check networking/reconnect, suspend/resume, memory
   and power-loss durability before widening feature scope.

No real account has been logged in during automated validation.

Login-form tests can be compiled with the same host compiler flags using
`tests/login_form_test.cpp src/ui/login_form.cpp`. The Vita auth harness includes
synthetic state tests for rejected codes/passwords, 2FA hints, Ready confirmation
and account-profile cleanup. They never create or authorize an account.

For embedded-mode builds, `tests/app_credentials_test.cpp` can be compiled with
`src/telegram/app_credentials.cpp src/telegram/credentials.cpp` and include paths
`-Isrc -Ibuild/generated`. Pass the private source-config path at runtime; the
test compares values without displaying them and asserts production selection.
The compiled test executable contains app credentials and belongs in ignored
build/ or temporary storage, not source control.
