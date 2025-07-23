
"use client";

import type { FileContent } from "@/lib/types";
import { WaveFileViewer } from "./wave-file-viewer";

export function FileContentViewer({ fileContent, isDataLoading }: { fileContent: FileContent, isDataLoading: boolean }) {
  if (fileContent.isBinary) {
     return <WaveFileViewer fileContent={fileContent} isDataLoading={isDataLoading}/>;
  }
  
  // Fallback for non-audio text-based files like JSON, CSV, TXT
  // This can be expanded to include visualizers for these types too.
  return (
    <div className="p-4 bg-muted h-full rounded-lg">
      <h3 className="font-semibold text-lg mb-2">Raw File Content</h3>
      <pre className="text-sm whitespace-pre-wrap">{fileContent.content}</pre>
    </div>
  );
}
