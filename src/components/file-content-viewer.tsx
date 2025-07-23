
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

  return (
    <Tabs defaultValue="pretty" className="h-full flex flex-col">
      <TabsList>
        <TabsTrigger value="pretty" disabled={!hasVisualization && fileContent.isBinary}>
          Visualize
        </TabsTrigger>
        <TabsTrigger value="raw">Raw Content</TabsTrigger>
      </TabsList>

      <TabsContent value="pretty" className="flex-grow h-0">
        {hasVisualization ? (
          <DataVisualizer data={fileContent.extractedData} />
        ) : (
          <div className="flex items-center justify-center h-full text-muted-foreground">
            No structured data to visualize.
          </div>
        )}
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
