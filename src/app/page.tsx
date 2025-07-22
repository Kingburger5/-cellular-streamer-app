import { getFilesAction } from "@/app/actions";
import { MainView } from "@/components/main-view";

export default async function Home() {
  const initialFiles = await getFilesAction();

  return <MainView initialFiles={initialFiles} />;
}
