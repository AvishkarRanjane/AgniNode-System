const $ = (s) => document.querySelector(s);
(async () => {
  const err = $('#err');
  const go = () => location.replace('dashboard.html');

  try {
    const B = 'https://www.gstatic.com/firebasejs/10.12.2/';
    const [{ initializeApp }, A] = await Promise.all([
      import(B + 'firebase-app.js'),
      import(B + 'firebase-auth.js'),
    ]);
    const auth = A.getAuth(initializeApp(CFG));

    A.onAuthStateChanged(auth, (u) => {
      if (u) {
        sessionStorage.removeItem('demoRole');
        go();
      }
    });

    $('#real').onsubmit = async (e) => {
      e.preventDefault();
      err.textContent = '';
      const email = $('#em').value.trim();
      const pwd = $('#pw').value;
      try {
        await A.signInWithEmailAndPassword(auth, email, pwd);
      } catch (ex) {
        err.textContent = 'Authentication failed: ' + ex.message;
      }
    };

    $('#reset').onclick = async () => {
      const email = $('#em').value.trim();
      if (!email) {
        err.textContent = 'Please enter your email address first.';
        return;
      }
      try {
        await A.sendPasswordResetEmail(auth, email);
        err.textContent = 'Password reset email sent! Check your inbox.';
        err.style.color = '#10b981';
      } catch (ex) {
        err.textContent = 'Could not send reset email: ' + ex.message;
        err.style.color = '#ff4d5a';
      }
    };
  } catch (ex) {
    err.textContent = 'Could not load Firebase Auth: ' + ex.message;
  }
})();
