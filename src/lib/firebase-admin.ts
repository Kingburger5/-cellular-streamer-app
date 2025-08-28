
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

const serviceAccountKey = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;
const bucketName = process.env.NEXT_PUBLIC_FIREBASE_STORAGE_BUCKET;

if (!getApps().length) {
    if (serviceAccountKey) {
        // Running in a deployed environment with the secret set
        const serviceAccount = JSON.parse(serviceAccountKey) as ServiceAccount;
        adminApp = initializeApp({
            credential: cert(serviceAccount),
            storageBucket: bucketName
        });
    } else {
        // Running locally or in an environment with default credentials
        adminApp = initializeApp({
             storageBucket: bucketName
        });
    }
} else {
    adminApp = getApps()[0];
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
