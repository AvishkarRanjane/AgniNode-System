# Downloads Google Trust Services root CAs (used by *.googleapis.com) once, at build time.
# Append other CA PEMs to certs/roots.pem if your OTA firmware host uses a different CA.
Import("env")
import os, urllib.request

path = os.path.join(env["PROJECT_DIR"], "certs", "roots.pem")
if not os.path.isfile(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        print("Fetching https://pki.goog/roots.pem ...")
        urllib.request.urlretrieve("https://pki.goog/roots.pem", path)
    except Exception as e:
        raise SystemExit(
            "Could not download roots.pem (%s). Save https://pki.goog/roots.pem to certs/roots.pem manually."
            % e
        )
