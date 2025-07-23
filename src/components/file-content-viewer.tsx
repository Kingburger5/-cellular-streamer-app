
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { WaveFileViewer } from "./wave-file-viewer";
import { DataVisualizer } from "./data-visualizer";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;
  const isWav = fileContent.extension === '.wav';

  if (isWav) {
    return <WaveFileViewer fileContent={fileContent} />;
  }

  // Fallback for non-wav files like JSON, CSV, TXT
  if (hasVisualization) {
    return <DataVisualizer data={fileContent.extractedData} />;
  }
  
  return (
     <Card className="h-full">
        <CardHeader>
            <CardTitle>Raw File Content</CardTitle>
        </CardHeader>
        <CardContent>
             <pre className="text-sm whitespace-pre-wrap">{fileContent.content}</pre>
        </CardContent>
     </Card>
  );
}
