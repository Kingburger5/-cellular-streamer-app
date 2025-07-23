
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

/**
 * Searches for a GUANO metadata chunk within a WAV file buffer by parsing the RIFF chunk structure.
 * This is a more reliable method than a simple string search.
 * @param buffer The WAV file content as a Buffer.
 * @returns The GUANO metadata string if found, otherwise null.
 */
function findGuanoMetadata(buffer: Buffer): string | null {
  try {
    // Check for 'RIFF' and 'WAVE' headers
    if (buffer.toString('ascii', 0, 4) !== 'RIFF' || buffer.toString('ascii', 8, 12) !== 'WAVE') {
      console.error("Not a valid RIFF/WAVE file.");
      return null;
    }

    let offset = 12; // Start scanning after the main RIFF header

    while (offset < buffer.length - 8) { // -8 to ensure we can read a full chunk header
      const chunkId = buffer.toString('ascii', offset, offset + 4);
      const chunkSize = buffer.readUInt32LE(offset + 4);
      
      offset += 8; // Move pointer to the start of the chunk data

      if (chunkId.trim() === 'GUANO') {
        if (offset + chunkSize > buffer.length) {
          console.error("GUANO chunk size is larger than the remaining file.");
          return null;
        }
        const guanoData = buffer.subarray(offset, offset + chunkSize);
        // The metadata is often null-terminated. Find the first null character.
        const nullCharIndex = guanoData.indexOf(0);
        const end = nullCharIndex !== -1 ? nullCharIndex : guanoData.length;
        return guanoData.subarray(0, end).toString('utf-8').trim();
      }

      // Move to the next chunk. Chunk size must be even for RIFF format.
      const nextOffset = offset + chunkSize;
      offset = nextOffset % 2 === 0 ? nextOffset : nextOffset + 1; // Align to word boundary
    }

    return null; // GUANO chunk not found
  } catch (error) {
    console.error("Error parsing WAV file for GUANO metadata:", error);
    return null;
  }
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

    if (['.wav', '.mp3', 'ogg'].includes(extension)) {
        isBinary = true;
        content = `data:audio/wav;base64,${fileBuffer.toString('base64')}`;
        rawMetadata = findGuanoMetadata(fileBuffer);
        
        if (rawMetadata) {
            // No AI parsing for now, just show the raw metadata.
        }

    } else {
        content = fileBuffer.toString("utf-8");
        rawMetadata = content; // The whole file is the metadata
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
