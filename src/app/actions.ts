
"use server";

import fs from "fs/promises";
import path from "path";
import { extractData } from "@/ai/flows/extract-data-flow";
import { appendToSheetFlow } from "@/ai/flows/append-to-sheet-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// Helper to ensure upload directory exists
async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("Error creating upload directory:", error);
      throw error;
    }
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
    if (error instanceof Error && 'code' in error && error.code === 'ENOENT') {
        return [];
    }
    throw error;
  }
}


function findReadableStrings(buffer: Buffer): string | null {
    const guanoKeyword = Buffer.from("GUANO");
    const guanoIndex = buffer.indexOf(guanoKeyword);

    if (guanoIndex === -1) {
        return null;
    }

    // Search for the end of the metadata block, which we'll assume is a null terminator or series of non-printable chars
    let endIndex = guanoIndex + guanoKeyword.length;
    while(endIndex < buffer.length) {
        const charCode = buffer[endIndex];
        // Stop at the first non-printable character (excluding newline/carriage return)
        if ( (charCode < 32 || charCode > 126) && charCode !== 10 && charCode !== 13) {
            break;
        }
        endIndex++;
    }
    
    return buffer.toString('utf-8', guanoIndex, endIndex).trim();
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

export async function getExtractedDataAction(
    rawMetadata: string | null,
    filename: string
): Promise<DataPoint[] | null> {
    if (!rawMetadata) {
        return null;
    }
    try {
        const aiResult = await extractData({ fileContent: rawMetadata, filename: filename });

        if (aiResult && aiResult.data.length > 0) {
            // After successful extraction, trigger the Google Sheet update for the first data point.
            // This assumes one primary data point per file.
            const dataPoint = aiResult.data[0];
            await appendToSheetFlow({ dataPoint: dataPoint, originalFilename: filename });
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
