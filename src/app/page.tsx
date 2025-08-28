
import { MainView } from "@/components/main-view";
import { getFilesAction } from "@/app/actions";

export default async function Home() {
  // initialFiles is now fetched on the client-side to avoid server auth issues.
  // We pass an empty array to the component to start.
  return <MainView initialFiles={[]} />;
}
