
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
 * Searches for a GUANO metadata chunk within a WAV file buffer.
 * This function parses the RIFF chunk structure to reliably find the metadata.
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

    let offset = 12; // Start after the RIFF header

    // Loop through all chunks
    while (offset < buffer.length) {
      const chunkId = buffer.toString('ascii', offset, offset + 4);
      const chunkSize = buffer.readUInt32LE(offset + 4);
      
      offset += 8; // Move pointer past chunk header

      if (chunkId.trim() === 'GUANO') {
        // Found the GUANO chunk. The actual metadata might be null-terminated.
        const guanoData = buffer.subarray(offset, offset + chunkSize);
        // Find the first null character, as the chunk can be padded.
        const nullCharIndex = guanoData.indexOf(0);
        const end = nullCharIndex !== -1 ? nullCharIndex : guanoData.length;
        return guanoData.subarray(0, end).toString('utf-8').trim();
      }

      // Move to the next chunk. Chunk size must be even.
      const nextOffset = offset + chunkSize;
      offset = nextOffset % 2 === 0 ? nextOffset : nextOffset + 1;
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
        // For audio playback, we need the entire file content as base64
        content = fileBuffer.toString('base64');
        rawMetadata = findGuanoMetadata(fileBuffer);
        
        if (rawMetadata) {
            try {
                const result = await extractData({ fileContent: rawMetadata });
                if (result.data && result.data.length > 0) {
                  extractedData = result.data;
                }
            } catch (e) {
                console.error("Could not extract metadata from audio file:", e);
                // Fail gracefully, extractedData will remain null
            }
        }

    } else {
        content = fileBuffer.toString("utf-8");
        // For non-audio files, we can also try to extract data
        try {
            const result = await extractData({ fileContent: content });
            if (result.data && result.data.length > 0) {
                extractedData = result.data;
                rawMetadata = content; // The whole file is the metadata
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
