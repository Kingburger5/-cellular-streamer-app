
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
    // GUANO metadata is a UTF-8 string that starts with "GUANO" and is part of a chunk.
    const guanoHeader = Buffer.from("GUANO");
    const headerIndex = buffer.indexOf(guanoHeader);

    if (headerIndex === -1) {
        return null;
    }
    
    // The metadata is often in a 'junk' chunk before the 'data' chunk.
    // Let's find the end of the metadata. It's often terminated by the start of the next RIFF chunk.
    // A simple way is to read line by line until we no longer have valid text.
    
    const endOfSearch = headerIndex + 1024; // Search within a reasonable range after the header
    let metadataBlock = buffer.toString('utf-8', headerIndex, endOfSearch);

    // Clean up null characters that might terminate the string early
    metadataBlock = metadataBlock.replace(/\0/g, '').trim();

    // The GUANO block itself is often a single line. We find that line.
    const lines = metadataBlock.split(/(\r\n|\n|\r)/);
    const guanoLine = lines.find(line => line.startsWith("GUANO"));

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
            const textContent = findGuanoMetadata(fileBuffer);
            if (textContent) {
                console.log("Found GUANO metadata:", textContent);
                const result = await extractData({ fileContent: textContent });
                if (result.data && result.data.length > 0) {
                  extractedData = result.data;
                } else {
                  console.log("AI failed to extract data from GUANO block.");
                }
            } else {
               console.log("No GUANO metadata found in file.");
            }
        } catch (e) {
            console.error("Could not extract metadata from audio file:", e);
        }

    } else {
        content = fileBuffer.toString("utf-8");
        try {
            const result = await extractData({ fileContent: content });
            if (result.data && result.data.length > 0) {
                extractedData = result.data;
            }
        } catch (e) {
            console.error("Could not extract metadata from text file:", e);
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
