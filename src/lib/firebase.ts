// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getStorage } from "firebase/storage";
import { getDatabase } from "firebase/database";

// Your web app's Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyDWxIExVVeOR4eMaG8rYvb_svCquk8vWSg",
  authDomain: "cellular-data-streamer.firebaseapp.com",
  projectId: "cellular-data-streamer",
  storageBucket: "cellular-data-streamer.appspot.com",
  messagingSenderId: "945649809294",
  appId: "1:945649809294:web:00c9ea813d2fc49d6d16df",
  databaseURL: "https://cellular-data-streamer-default-rtdb.firebaseio.com"
};

// Initialize Firebase
export const app = initializeApp(firebaseConfig);
export const storage = getStorage(app);
export const database = getDatabase(app);
