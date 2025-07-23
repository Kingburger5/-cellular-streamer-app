
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
  return (
    <Tabs defaultValue="visualize" className="h-full flex flex-col">
      <TabsList>
        <TabsTrigger value="visualize" disabled={!hasVisualization}>
          Visualize
        </TabsTrigger>
        <TabsTrigger value="raw">Raw Content</TabsTrigger>
      </TabsList>

      <TabsContent value="visualize" className="flex-grow h-0">
          <DataVisualizer data={fileContent.extractedData} />
      </TabsContent>
      <TabsContent value="raw" className="flex-grow h-0">
         <Card className="h-full">
            <CardHeader>
                <CardTitle>Raw File Content</CardTitle>
            </CardHeader>
            <CardContent>
                 <pre className="text-sm whitespace-pre-wrap">{fileContent.content}</pre>
            </CardContent>
         </Card>
      </TabsContent>
    </Tabs>
  );
}
