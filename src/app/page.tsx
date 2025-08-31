
import { MainView } from "@/components/main-view";

export default async function Home() {
  // initialFiles is now fetched on the client-side to avoid server auth issues
  // and prevent server-rendering errors during SDK initialization.
  // We pass an empty array to the component to start.
  return <MainView initialFiles={[]} />;
}
