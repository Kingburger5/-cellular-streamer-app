"use server";

import fs from "fs/promises";
import path from "path";
import { summarizeFile } from "@/ai/flows/summarize-file";
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
    let textContentForSummary = "";

    if (['.wav', '.mp3', '.ogg'].includes(extension)) {
        content = fileBuffer.toString('base64');
        isBinary = true;
        
        try {
            const textContent = fileBuffer.toString('utf-8');
            textContentForSummary = textContent; // Use this for summary later
            const result = await extractData({ fileContent: textContent });
            if (result.data && result.data.length > 0) {
              extractedData = result.data;
            }
        } catch (e) {
            console.log("Could not extract metadata from audio file:", e);
        }

    } else {
        content = fileBuffer.toString("utf-8");
        textContentForSummary = content;
    }

    return { content, extension, name: sanitizedFilename, isBinary, extractedData, textContentForSummary };
  } catch (error) {
    console.error(`Error reading file ${filename}:`, error);
    return null;
  }
}

export async function generateSummaryAction(input: {
  filename: string;
  fileContent: string;
}): Promise<{ summary: string } | { error: string }> {
  try {
    const result = await summarizeFile({
        filename: input.filename,
        fileContent: input.fileContent,
    });
    return result;
  } catch (error) {
    console.error("Error generating summary:", error);
    return { error: "Failed to generate summary." };
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
