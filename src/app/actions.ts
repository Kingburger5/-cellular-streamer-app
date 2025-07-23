
"use server";

import fs from "fs/promises";
import path from "path";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent } from "@/lib/types";

const UPLOAD_DIR = "/tmp/uploads";

async function ensureUploadDirExists() {
  try {
    await fs.access(UPLOAD_DIR);
  } catch {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  }
}

export async function getFilesAction(): Promise<UploadedFile[]> {
  await ensureUploadDirExists();
  try {
    const filenames = await fs.readdir(UPLOAD_DIR);
    const filesWithStats = await Promise.all(
      filenames.map(async (name) => {
        const filePath = path.join(UPLOAD_DIR, name);
        const stats = await fs.stat(filePath);
        return {
          name,
          size: stats.size,
          uploadDate: stats.mtime,
        };
      })
    );

    filesWithStats.sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());

    return filesWithStats;
  } catch (error) {
    console.error("Error reading files:", error);
    return [];
  }
}

function findGuanoMetadata(buffer: Buffer): string | null {
    const guanoHeader = Buffer.from("GUANO");
    const headerIndex = buffer.indexOf(guanoHeader);

    if (headerIndex === -1) {
        return null;
    }
    
    // Search for the end of the metadata block. GUANO metadata is a single line terminated by a newline or null character.
    const searchEnd = Math.min(headerIndex + 2048, buffer.length); // Limit search to 2KB
    let metadataEnd = -1;

    for (let i = headerIndex; i < searchEnd; i++) {
        // Look for newline or null terminator
        if (buffer[i] === 0x0A || buffer[i] === 0x00) {
            metadataEnd = i;
            break;
        }
    }

    if (metadataEnd === -1) {
       metadataEnd = searchEnd;
    }

    // Extract the line containing the GUANO metadata.
    let metadataBlock = buffer.toString('utf-8', headerIndex, metadataEnd).trim();
    
    // Ensure we only return the line that starts with GUANO
    if (metadataBlock.startsWith("GUANO")) {
        return metadataBlock;
    }

    return null;
}

export async function getFileContentAction(
  filename: string
): Promise<FileContent | null> {
  try {
    const sanitizedFilename = path.basename(filename);
    if (sanitizedFilename !== filename) {
      throw new Error("Invalid filename.");
    }

    const filePath = path.join(UPLOAD_DIR, sanitizedFilename);
    const extension = path.extname(sanitizedFilename).toLowerCase();
    
    const fileBuffer = await fs.readFile(filePath);
    let content: string;
    let isBinary = false;
    let extractedData = null;
    let rawMetadata: string | null = null;

    if (['.wav', '.mp3', '.ogg'].includes(extension)) {
        content = "Binary audio file. Content not displayed.";
        isBinary = true;
        
        try {
            const textContent = findGuanoMetadata(fileBuffer);
            rawMetadata = textContent; // Save the raw metadata string
            if (textContent) {
                const result = await extractData({ fileContent: textContent });
                if (result.data && result.data.length > 0) {
                  extractedData = result.data;
                }
            }
        } catch (e) {
            console.error("Could not extract metadata from audio file:", e);
        }

    } else {
        content = fileBuffer.toString("utf-8");
        rawMetadata = content;
        try {
            const result = await extractData({ fileContent: content });
            if (result.data && result.data.length > 0) {
                extractedData = result.data;
            }
        } catch (e) {
            console.error("Could not extract metadata from text file:", e);
        }
    }

    return { content, extension, name: sanitizedFilename, isBinary, extractedData, rawMetadata };
  } catch (error) {
    console.error(`Error reading file ${filename}:`, error);
    return null;
  }
}

export async function deleteFileAction(
  filename: string
): Promise<{ success: true } | { error: string }> {
  try {
    const sanitizedFilename = path.basename(filename);
    if (sanitizedFilename !== filename) {
      throw new Error("Invalid filename.");
    }

    const filePath = path.join(UPLOAD_DIR, sanitizedFilename);
    await fs.unlink(filePath);
    return { success: true };
  } catch (error) {
    console.error(`Error deleting file ${filename}:`, error);
    if (error instanceof Error) {
       return { error: `Failed to delete file: ${error.message}` };
    }
    return { error: "Failed to delete file." };
  }
}
