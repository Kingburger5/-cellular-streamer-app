import * as admin from 'firebase-admin';

let adminApp: admin.app.App;

if (!admin.apps.length) {
  adminApp = admin.initializeApp({
    storageBucket: process.env.NEXT_PUBLIC_FIREBASE_STORAGE_BUCKET
  });
} else {
  adminApp = admin.app();
}

export { adminApp };
