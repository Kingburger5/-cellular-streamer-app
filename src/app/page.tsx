
import { MainView } from "@/components/main-view";
import { getFilesAction } from "@/app/actions";

export default async function Home() {
  const initialFiles = await getFilesAction();
  return <MainView initialFiles={initialFiles} />;
}
