import { initializeApp, getApps } from "firebase/app";

const firebaseConfig = {
  projectId: "cellular-data-streamer",
  appId: "1:945649809294:web:00c9ea813d2fc49d6d16df",
  storageBucket: "cellular-data-streamer.firebasestorage.app",
  apiKey: "AIzaSyDWxIExVVeOR4eMaG8rYvb_svCquk8vWSg",
  authDomain: "cellular-data-streamer.firebaseapp.com",
  measurementId: "",
  messagingSenderId: "945649809294",
};

// Initialize Firebase
let firebaseApp = getApps().length === 0 ? initializeApp(firebaseConfig) : getApps()[0];

export default firebaseApp;
