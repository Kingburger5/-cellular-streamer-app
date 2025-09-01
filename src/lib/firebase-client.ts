
import { initializeApp, getApps, FirebaseApp } from "firebase/app";
import { getStorage, ref, listAll, getMetadata } from "firebase/storage";
import { getAuth, signInAnonymously, onAuthStateChanged, User } from "firebase/auth";
import type { UploadedFile } from "./types";

const firebaseConfig = {
  apiKey: "AIzaSyDWxIExVVeOR4eMaG8rYvb_svCquk8vWSg",
  authDomain: "cellular-data-streamer.firebaseapp.com",
  projectId: "cellular-data-streamer",
  storageBucket: "cellular-data-streamer.firebasestorage.app",
  messagingSenderId: "945649809294",
  appId: "1:945649809294:web:81866a3f9e7734a96d16df"
};


let app: FirebaseApp;
if (!getApps().length) {
  app = initializeApp(firebaseConfig);
} else {
  app = getApps()[0];
}

const storage = getStorage(app);
const auth = getAuth(app);

// Helper function to ensure user is signed in
let authReady: Promise<User | null>;
let authResolved = false;

const ensureSignIn = (): Promise<User | null> => {
  if (!authReady) {
    authReady = new Promise((resolve, reject) => {
      // This listener will be called whenever the auth state changes.
      const unsubscribe = onAuthStateChanged(auth, async (user) => {
        if (user) {
          if (!authResolved) {
            authResolved = true;
            resolve(user);
          }
          unsubscribe(); // Stop listening after we have a user.
        } else if (!auth.currentUser && !authResolved) {
          // If there's no user and we haven't already tried, sign in.
          try {
            await signInAnonymously(auth);
            // The onAuthStateChanged listener will be triggered again with the new user.
          } catch (error) {
            console.error("Anonymous sign-in failed:", error);
            if (!authResolved) {
              authResolved = true;
              reject(error);
            }
            unsubscribe();
          }
        }
      }, (error) => {
          console.error("Auth state change error:", error);
          if (!authResolved) {
            authResolved = true;
            reject(error);
          }
          unsubscribe();
      });
    });
  }
  return authReady;
};


export async function getClientFiles(): Promise<UploadedFile[]> {
  try {
    await ensureSignIn(); // Make sure we are authenticated before making a request
    
    const listRef = ref(storage, 'uploads');
    const res = await listAll(listRef);

    const fileDetails = await Promise.all(
      res.items.map(async (itemRef) => {
        const metadata = await getMetadata(itemRef);
        // Use `itemRef.name` to get just the filename (e.g., "file.wav")
        // `itemRef.fullPath` would be "uploads/file.wav"
        const name = itemRef.name;
        return {
          name: name,
          size: metadata.size,
          uploadDate: new Date(metadata.timeCreated),
        };
      })
    );

    return fileDetails.sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());
  } catch (error: any) {
    console.error("[Client] Error fetching files:", error);
    // Provide a more user-friendly error message
    if (error.code === 'storage/unauthorized' || error.code === 'storage/object-not-found') {
      throw new Error('Permission denied. Please check your Storage Security Rules in the Firebase Console to ensure read access is allowed for authenticated users in the `uploads` folder.');
    }
    throw new Error("Could not fetch files from storage.");
  }
}

export { app as clientApp, storage as clientStorage, auth as clientAuth };
