
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { Buffer } from "buffer";


let adminApp: App | null = null;
let adminStorage: Storage | null = null;

function initializeFirebaseAdmin() {
    if (adminApp) {
        return { adminApp, adminStorage: adminStorage! };
    }

    const appName = 'firebase-admin-app-cellular-streamer';
    const existingApp = getApps().find(app => app.name === appName);

    if (existingApp) {
        adminApp = existingApp;
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        
        // Check if the secret is missing, empty, or just whitespace.
        // This is the condition for local development.
        if (!serviceAccountString || serviceAccountString.trim() === '') {
             // Local development environment (relies on ADC from `gcloud auth application-default login`)
            console.log("[INFO] No GOOGLE_APPLICATION_CREDENTIALS_JSON secret found. Using Application Default Credentials for local development.");
            adminApp = initializeApp({
                storageBucket: "cellular-data-streamer.firebasestorage.app"
            }, appName);

        } else {
            // Production environment (App Hosting with secret)
            console.log("[INFO] Initializing Firebase Admin SDK with service account from secret.");
            
            console.log(
              "[DEBUG] Secret first 100 chars:",
              JSON.stringify(serviceAccountString.slice(0, 100))
            );
            
            let decodedString = serviceAccountString;
            // A heuristic to check for base64. A more robust check might be needed if other strings trigger this.
            if (/^[a-zA-Z0-9+/=]+$/.test(decodedString.trim())) {
                try {
                    const fromBase64 = Buffer.from(decodedString, "base64").toString("utf8");
                     // If decoding results in a string that looks like JSON, use it.
                    if (fromBase64.trim().startsWith('{')) {
                        console.log("[INFO] Secret appears to be Base64-encoded. Using decoded value.");
                        decodedString = fromBase64;
                    }
                } catch (e) {
                    console.log("[INFO] Could not decode secret from base64, using as-is.", e);
                }
            }

            let serviceAccount: ServiceAccount;
            try {
                // Try parsing once for standard JSON
                serviceAccount = JSON.parse(decodedString);
            } catch (e) {
                // If that fails, it's likely double-encoded. Parse it again.
                console.log("[INFO] Direct parse failed. Attempting to parse double-encoded JSON.");
                serviceAccount = JSON.parse(JSON.parse(decodedString));
            }
            
            // The critical fix for the "Invalid PEM" error caused by environment variable escaping.
            // Note: The type expects `privateKey` (camelCase) but the JSON from Google uses `private_key` (snake_case).
            // We cast to `any` to handle this discrepancy without TypeScript errors.
            const aServiceAccount = serviceAccount as any;
            if (aServiceAccount.private_key) {
                aServiceAccount.private_key = aServiceAccount.private_key.replace(/\\n/g, '\n');
            }

            adminApp = initializeApp({
                credential: cert(serviceAccount),
                storageBucket: "cellular-data-streamer.firebasestorage.app"
            }, appName);
        }
        
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error);
        // Provide a clearer error message that helps diagnose the issue
        const detail = error.message.includes("Invalid PEM") 
            ? "The private key in the service account credentials is malformed. This often happens due to incorrect newline character escaping when passed as an environment variable."
            : error.message.includes("JSON") 
            ? `The service account credentials from the secret are not valid JSON. Details: ${error.message}`
            : error.message;
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs. Detail: ${detail}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

export function getAdminStorage(): Storage {
    if (!adminStorage) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}
