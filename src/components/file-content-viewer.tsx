"use client";

import type { FileContent } from "@/lib/types";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { ScrollArea } from "./ui/scroll-area";
import { DataVisualizer } from "./data-visualizer";
import { WaveFileViewer } from "./wave-file-viewer";

function CsvViewer({ content }: { content: string }) {
  const rows = content.split("\n").filter(Boolean);
  if (rows.length === 0) {
    return <p className="text-muted-foreground">Empty CSV file.</p>;
  }
  const headers = rows[0].split(",");
  const body = rows.slice(1).map(row => row.split(","));

  return (
    <ScrollArea className="h-full">
      <Table className="w-full">
        <TableHeader>
          <TableRow>
            {headers.map((header, i) => (
              <TableHead key={`${header}-${i}`}>{header.trim()}</TableHead>
            ))}
          </TableRow>
        </TableHeader>
        <TableBody>
          {body.map((row, i) => (
            <TableRow key={`row-${i}`}>
              {row.map((cell, j) => (
                <TableCell key={`cell-${i}-${j}`}>{cell.trim()}</TableCell>
              ))}
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </ScrollArea>
  );
}

function JsonViewer({ content }: { content: string }) {
  try {
    const parsed = JSON.parse(content);
    return <pre className="text-sm">{JSON.stringify(parsed, null, 2)}</pre>;
  } catch {
    return <pre className="text-sm text-destructive">{content}</pre>;
  }
}

function TxtViewer({ content }: { content: string }) {
  return <pre className="text-sm whitespace-pre-wrap">{content}</pre>;
}


export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const hasPrettyView = [".json", ".csv", ".txt"].includes(fileContent.extension);
  const isDataVis = [".json", ".csv"].includes(fileContent.extension) || (fileContent.extension === '.wav' && !!fileContent.extractedData);
  const isAudio = fileContent.extension === ".wav";
  const hasVisualization = isDataVis || isAudio;

  const defaultTab = hasVisualization ? "visualize" : (hasPrettyView ? "pretty" : "raw");

  return (
    <Tabs defaultValue={defaultTab} className="flex flex-col h-full">
      <TabsList className={`grid w-full ${hasVisualization && hasPrettyView ? "grid-cols-3" : hasVisualization || hasPrettyView ? "grid-cols-2" : "grid-cols-1"} mb-4`}>
        {hasVisualization && <TabsTrigger value="visualize">Visualize</TabsTrigger>}
        {hasPrettyView && (
           <TabsTrigger value="pretty">Pretty</TabsTrigger>
        )}
        <TabsTrigger value="raw">Raw</TabsTrigger>
      </TabsList>
       {hasVisualization && (
        <TabsContent value="visualize" className="flex-grow h-0">
          {isAudio ? <WaveFileViewer fileContent={fileContent} /> : <DataVisualizer data={null} rawFileContent={fileContent} />}
        </TabsContent>
      )}
      {hasPrettyView && (
         <TabsContent value="pretty" className="flex-grow h-0">
          {fileContent.extension === ".csv" && <CsvViewer content={fileContent.content} />}
          {fileContent.extension === ".json" && <JsonViewer content={fileContent.content} />}
          {fileContent.extension === ".txt" && <TxtViewer content={fileContent.content} />}
        </TabsContent>
      )}
      <TabsContent value="raw" className="flex-grow h-0">
        <TxtViewer content={fileContent.isBinary ? "Binary content not displayed." : fileContent.content} />
      </TabsContent>
    </Tabs>
  );
}
