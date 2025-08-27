
"use server";

import { extractData } from "@/ai/flows/extract-data-flow";
import { appendToSheet } from "@/ai/flows/append-to-sheet-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

// This is a placeholder as we are not saving files to disk anymore.
// The concept of "getting files" will need to be re-evaluated.
// For now, it returns an empty array.
export async function getFilesAction(): Promise<UploadedFile[]> {
  return [];
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


export async function processUploadedFileAction(
  fileBuffer: ArrayBuffer,
  originalFilename: string,
): Promise<FileContent | null> {
    try {
        const buffer = Buffer.from(fileBuffer);
        const extension = originalFilename.split('.').pop()?.toLowerCase() || '';

        let content: string;
        let isBinary = false;
        let rawMetadata: string | null = null;
        let extractedData: DataPoint[] | null = null;

        if (['wav', 'mp3', 'ogg'].includes(extension)) {
            isBinary = true;
            content = `data:audio/wav;base64,${buffer.toString('base64')}`;
            rawMetadata = findReadableStrings(buffer);
        } else {
            content = buffer.toString("utf-8");
            rawMetadata = content;
        }
        
        if (rawMetadata) {
            const aiResult = await extractData({ fileContent: rawMetadata, filename: originalFilename });
             if (aiResult && aiResult.data.length > 0) {
                extractedData = aiResult.data;
                // After successful extraction, trigger the Google Sheet update for the first data point.
                const dataPoint = aiResult.data[0];
                await appendToSheet({ dataPoint: dataPoint, originalFilename: originalFilename });
            }
        }

        return { content, extension, name: originalFilename, isBinary, rawMetadata, extractedData };

    } catch(error) {
        console.error(`Error processing file ${originalFilename}:`, error);
        return null;
    }
}


// The delete action is no longer needed as we don't store files.
export async function deleteFileAction(
  filename: string
): Promise<{ success: true } | { error: string }> {
  console.log(`Received delete request for ${filename}, but files are no longer stored.`);
  return { success: true };
}

// Logs are now viewed in the Firebase Console, not from a file.
export async function getLogsAction(): Promise<string> {
    return "Connection logs are now available in the Firebase Console under the 'Logs' tab for your App Hosting backend. Local file logging has been disabled.";
}
