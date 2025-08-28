
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
let currentUser: User | null = null;
onAuthStateChanged(auth, (user) => {
    currentUser = user;
});

const ensureSignIn = async (): Promise<User> => {
    if (currentUser) {
        return currentUser;
    }
    const userCredential = await signInAnonymously(auth);
    return userCredential.user;
}

export async function getClientFiles(): Promise<UploadedFile[]> {
  try {
    await ensureSignIn(); // Make sure we are authenticated before making a request
    
    const listRef = ref(storage, 'uploads');
    const res = await listAll(listRef);

    const fileDetails = await Promise.all(
      res.items.map(async (itemRef) => {
        const metadata = await getMetadata(itemRef);
        return {
          name: metadata.name,
          size: metadata.size,
          uploadDate: new Date(metadata.timeCreated),
        };
      })
    );

    return fileDetails.sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());
  } catch (error: any) {
    console.error("[Client] Error fetching files:", error);
    // Provide a more user-friendly error message
    if (error.code === 'storage/unauthorized') {
      throw new Error('Permission denied. Please check your Storage Security Rules in the Firebase Console to ensure read access is allowed for authenticated users.');
    }
    throw new Error("Could not fetch files from storage.");
  }
}

export { app as clientApp, storage as clientStorage, auth as clientAuth };
