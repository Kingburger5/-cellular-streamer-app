
"use server";

import fs from "fs/promises";
import path from "path";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

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
 * Searches for any readable text runs within a binary buffer.
 * A more robust way to find metadata when chunk parsing fails.
 * @param buffer The file content as a Buffer.
 * @returns The longest readable string found, which should be the GUANO metadata.
 */
function findReadableStrings(buffer: Buffer): string | null {
    let longestString = "";
    let currentString = "";
    // A character is considered printable if its ASCII code is between 32 (space) and 126 (~).
    // We also include newline (10) and carriage return (13) and pipe (124).
    const isPrintable = (char: number) => (char >= 32 && char <= 126) || char === 10 || char === 13;

    for (let i = 0; i < buffer.length; i++) {
        const charCode = buffer[i];
        if (isPrintable(charCode)) {
            currentString += String.fromCharCode(charCode);
        } else {
            if (currentString.includes("GUANO")) {
                 if (currentString.length > longestString.length) {
                    longestString = currentString;
                }
            }
            currentString = "";
        }
    }
    // Check one last time at the end of the buffer
    if (currentString.includes("GUANO")) {
        if (currentString.length > longestString.length) {
            longestString = currentString;
        }
    }
    
    // Clean up the output string if it contains the GUANO keyword
    if (longestString.includes("GUANO")) {
        const guanoIndex = longestString.indexOf("GUANO");
        // Trim anything before the GUANO string
        return longestString.substring(guanoIndex).trim();
    }
    
    return longestString || null;
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
    let rawMetadata: string | null = null;

    if (['.wav', '.mp3', '.ogg'].includes(extension)) {
        isBinary = true;
        content = `data:audio/wav;base64,${fileBuffer.toString('base64')}`;
        rawMetadata = findReadableStrings(fileBuffer);
    } else {
        content = fileBuffer.toString("utf-8");
        rawMetadata = content; // The whole file is the metadata
    }

    return { content, extension, name: sanitizedFilename, isBinary, rawMetadata, extractedData: null };
  } catch (error) {
    console.error(`Error reading file ${filename}:`, error);
    return null;
  }
}

export async function getExtractedDataAction(rawMetadata: string | null): Promise<DataPoint[] | null> {
    if (!rawMetadata) {
        return null;
    }
    try {
        const aiResult = await extractData({ fileContent: rawMetadata });
        if (aiResult && aiResult.data.length > 0) {
            return aiResult.data;
        }
        return null;
    } catch (error) {
        console.error("Error extracting data with AI:", error);
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
