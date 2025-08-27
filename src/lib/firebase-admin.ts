
import { initializeApp, getApps, App } from "firebase-admin/app";
import { getStorage } from "firebase-admin/storage";

// This is the one and only place Firebase Admin SDK is initialized.
// We are hardcoding the storage bucket to resolve a persistent
// environment variable issue.

let adminApp: App;

if (!getApps().length) {
  adminApp = initializeApp({
    storageBucket: 'cellular-data-streamer.appspot.com',
  });
} else {
  adminApp = getApps()[0];
}

const adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
