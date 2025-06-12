# How to Get a Gmail App Password for Email Sending

To allow this server to send emails through your Gmail account securely, you **must generate an App Password** from your Google account. Regular Gmail passwords **will not work**.

---

## Prerequisites

- A Google account.
- Two-Factor Authentication (2FA) **must be enabled** on your Google account.

---

## Steps to Generate a Gmail App Password

1. Go to https://myaccount.google.com/
2. Sign in with your Google account if you're not already signed in.
3. In the left-hand menu, click **Security**.
4. Under the **"How you sign in to Google"** section, ensure:
   - 2-Step Verification is **turned ON**.
5. Once 2FA is enabled, search for **App Passwords**.
6. In the app name you must pick a name to know what your app is gonna be called, something like:
   ```
   KalaServer Email
   ```
8. Click **Generate**.
9. Copy the 16-character password shown (`abcd efgh ijkl mnop`).
   - **Important:** This is your one-time chance to copy it.
10. Use this app password in the `EmailSenderData.password` field.

---

## Do Not Use Your Gmail Password

- App passwords are a safer alternative.
- They are **limited in scope** and can be **revoked any time** from your Google account settings.

---

## Where to Put the Credentials

Update your server's email config file (or environment variables) with:

```text
username | myemail@gmail.com
password | abcd efgh ijkl mnop
```

