
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// When running in a Google Cloud environment like App Hosting,
// the SDK will automatically use the service account of the environment.
// No credentials need to be passed explicitly.
if (!getApps().length) {
    adminApp = initializeApp();
} else {
    adminApp = getApps()[0];
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
