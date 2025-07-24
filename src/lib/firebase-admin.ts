import * as admin from 'firebase-admin';

let adminApp: admin.app.App;

if (!admin.apps.length) {
  adminApp = admin.initializeApp({
    credential: admin.credential.applicationDefault(),
    storageBucket: 'cellular-data-streamer.appspot.com'
  });
} else {
  adminApp = admin.app();
}

export { adminApp };
