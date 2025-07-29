
"use server";

import fs from "fs/promises";
import path from "path";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// Helper to ensure upload directory exists
async function ensureUploadDirExists() {
  try {
    await fs.access(UPLOAD_DIR);
  } catch {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  }
}

export async function getFilesAction(): Promise<UploadedFile[]> {
  try {
    await ensureUploadDirExists();
    const filenames = await fs.readdir(UPLOAD_DIR);
    const filesWithStats = await Promise.all(
      filenames.map(async (name) => {
        const filePath = path.join(UPLOAD_DIR, name);
        const stats = await fs.stat(filePath);
        return {
          name: name,
          size: stats.size,
          uploadDate: stats.mtime,
        };
      })
    );

    filesWithStats.sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());

    return filesWithStats;
  } catch (error) {
    console.error("Error reading files from upload directory:", error);
    return [];
  }
}


function findReadableStrings(buffer: Buffer): string | null {
    let longestString = "";
    let currentString = "";
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
    if (currentString.includes("GUANO")) {
        if (currentString.length > longestString.length) {
            longestString = currentString;
        }
    }
    
    if (longestString.includes("GUANO")) {
        const guanoIndex = longestString.indexOf("GUANO");
        return longestString.substring(guanoIndex).trim();
    }
    
    return longestString || null;
}


export async function getFileContentAction(
  filename: string
): Promise<FileContent | null> {
  try {
    await ensureUploadDirExists();
    const safeFilename = path.basename(filename);
    const filePath = path.join(UPLOAD_DIR, safeFilename);
    
    const extension = path.extname(safeFilename).toLowerCase();
    
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
        rawMetadata = content;
    }

    return { content, extension, name: safeFilename, isBinary, rawMetadata, extractedData: null };
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
    await ensureUploadDirExists();
    const safeFilename = path.basename(filename);
    const filePath = path.join(UPLOAD_DIR, safeFilename);
    await fs.unlink(filePath);
    console.log(`Successfully deleted ${safeFilename}`);
    return { success: true };
  } catch (error) {
    console.error(`Error deleting file ${filename}:`, error);
    if (error instanceof Error) {
       return { error: `Failed to delete file: ${error.message}` };
    }
    return { error: "Failed to delete file." };
  }
}
