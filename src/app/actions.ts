
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
    // GUANO metadata is a UTF-8 string that starts with "GUANO"
    const guanoHeader = "GUANO";
    const headerIndex = buffer.indexOf(guanoHeader);

    if (headerIndex === -1) {
        return null;
    }
    
    // Search for the end of the metadata block, which might be terminated by a null byte or the start of the 'data' chunk
    const dataChunkHeader = "data";
    const dataIndex = buffer.indexOf(dataChunkHeader, headerIndex);

    const endOfMeta = dataIndex !== -1 ? dataIndex : buffer.length;

    // Extract the potential metadata block and clean it up
    let metadata = buffer.toString('utf-8', headerIndex, endOfMeta);
    
    // Remove null characters and other non-printable characters that might follow the metadata
    metadata = metadata.replace(/\0/g, '').trim();

    // The metadata might be embedded within a 'junk' chunk, let's find the most relevant part
    const lines = metadata.split(/(\r\n|\n|\r)/);
    const guanoLine = lines.find(line => line.startsWith(guanoHeader));

    return guanoLine || null;
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

    if (['.wav', '.mp3', '.ogg'].includes(extension)) {
        content = fileBuffer.toString('base64');
        isBinary = true;
        
        try {
            // Attempt to find and decode readable text from the buffer for metadata
            const textContent = findGuanoMetadata(fileBuffer);
            if (textContent) {
                const result = await extractData({ fileContent: textContent });
                if (result.data && result.data.length > 0) {
                  extractedData = result.data;
                }
            }
        } catch (e) {
            console.log("Could not extract metadata from audio file:", e);
        }

    } else {
        content = fileBuffer.toString("utf-8");
        // For non-audio files, we can also try to extract data if they are text-based.
        const result = await extractData({ fileContent: content });
        if (result.data && result.data.length > 0) {
            extractedData = result.data;
        }
    }

    return { content, extension, name: sanitizedFilename, isBinary, extractedData };
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
