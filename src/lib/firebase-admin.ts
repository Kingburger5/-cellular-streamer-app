
import { initializeApp, getApps, App } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// When deployed to App Hosting, the SDK will automatically use the
// default service account credentials. No explicit configuration is needed.
if (!getApps().length) {
    adminApp = initializeApp({
        storageBucket: "cellular-data-streamer.firebasestorage.app",
    });
} else {
    adminApp = getApps()[0];
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
