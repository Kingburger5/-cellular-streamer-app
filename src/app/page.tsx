
import { MainView } from "@/components/main-view";

export default async function Home() {
  // Since we no longer read from the file system on the server,
  // we pass an empty array for initial files.
  // The UI is now fully client-driven based on user uploads.
  return <MainView initialFiles={[]} />;
}
