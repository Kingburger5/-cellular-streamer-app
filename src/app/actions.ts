
"use server";

import { adminApp } from "@/lib/firebase-admin";
import { getStorage } from "firebase-admin/storage";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

const bucket = getStorage(adminApp).bucket();

export async function getFilesAction(): Promise<UploadedFile[]> {
  try {
    const [files] = await bucket.getFiles();
    const filesWithStats = await Promise.all(
      files.map(async (file) => {
        const [metadata] = await file.getMetadata();
        return {
          name: file.name,
          size: Number(metadata.size),
          uploadDate: new Date(metadata.updated),
        };
      })
    );

    filesWithStats.sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());

    return filesWithStats;
  } catch (error) {
    console.error("Error reading files from Firebase Storage:", error);
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
    const file = bucket.file(filename);
    const [exists] = await file.exists();
    if (!exists) {
        throw new Error("File does not exist in Firebase Storage.");
    }
    
    const extension = `.${filename.split('.').pop()}`.toLowerCase();
    
    const [fileBuffer] = await file.download();
    
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

    return { content, extension, name: filename, isBinary, rawMetadata, extractedData: null };
  } catch (error) {
    console.error(`Error reading file ${filename} from Firebase Storage:`, error);
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
    const file = bucket.file(filename);
    await file.delete();
    console.log(`Successfully deleted ${filename} from Firebase Storage.`);
    return { success: true };
  } catch (error) {
    console.error(`Error deleting file ${filename} from Firebase Storage:`, error);
    if (error instanceof Error) {
       return { error: `Failed to delete file: ${error.message}` };
    }
    return { error: "Failed to delete file." };
  }
}
