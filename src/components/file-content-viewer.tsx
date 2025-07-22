"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { ScrollArea } from "./ui/scroll-area";

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
  const hasPrettyView = fileContent.extension === ".json" || fileContent.extension === ".csv";

  return (
    <Tabs defaultValue={hasPrettyView ? "pretty" : "raw"} className="flex flex-col h-full">
      <TabsList className="grid w-full grid-cols-2 mb-4">
        {hasPrettyView ? (
           <TabsTrigger value="pretty">Pretty</TabsTrigger>
        ) : <div />}
        <TabsTrigger value="raw">Raw</TabsTrigger>
      </TabsList>
      {hasPrettyView && (
         <TabsContent value="pretty" className="flex-grow h-0">
          {fileContent.extension === ".csv" && <CsvViewer content={fileContent.content} />}
          {fileContent.extension === ".json" && <JsonViewer content={fileContent.content} />}
        </TabsContent>
      )}
      <TabsContent value="raw" className="flex-grow h-0">
        <TxtViewer content={fileContent.content} />
      </TabsContent>
    </Tabs>
  );
}
