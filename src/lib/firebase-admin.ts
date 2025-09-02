
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
        
        // Check if the secret is missing or empty. This is the condition for local development.
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

            let serviceAccount: ServiceAccount;
            try {
                // First parse turns the string literal into a JSON string.
                let parsed = JSON.parse(serviceAccountString);

                // If the result is still a string (double-encoded), parse it again.
                // Otherwise, use the parsed object directly.
                serviceAccount = typeof parsed === 'string' ? JSON.parse(parsed) : parsed;
                
            } catch (e) {
                console.error("[CRITICAL] Failed to parse Firebase service account JSON from secret.", e);
                throw new Error("The service account secret is not valid JSON, even after attempting to parse it as single or double-encoded.");
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
