
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import { format } from 'date-fns';

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
const LOG_FILE = path.join(UPLOAD_DIR, "connections.log");

// Helper to ensure upload directory exists
async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("Error creating upload directory:", error);
      throw error; // rethrow
    }
  }
}

// Helper to log requests to a file
async function logRequest(request: NextRequest) {
    await ensureUploadDirExists();
    const timestamp = format(new Date(), "yyyy-MM-dd'T'HH:mm:ss.SSSxxx");
    let logEntry = `[${timestamp}] --- New Request ---\n`;
    
    // Get IP address, being mindful of proxy headers
    let ip = request.ip ?? request.headers.get('x-forwarded-for') ?? 'N/A';
    logEntry += `IP Address: ${ip}\n`;
    
    // Log all headers
    logEntry += "Headers:\n";
    request.headers.forEach((value, key) => {
        logEntry += `  ${key}: ${value}\n`;
    });
    logEntry += "---------------------------\n\n";

    try {
        await fs.appendFile(LOG_FILE, logEntry);
    } catch (logError) {
        console.error("Failed to write to log file:", logError);
    }
}


/**
 * Handles file uploads from devices using simple POST requests,
 * such as the SIM7600's AT+HTTPPOSTFILE command.
 *
 * This handler is not designed for chunked uploads. It treats each POST
 * request as a complete file.
 */
export async function POST(request: NextRequest) {
    
    await logRequest(request);

    try {
        await ensureUploadDirExists();

        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }
        
        const fileBuffer = await request.arrayBuffer();
        const buffer = Buffer.from(fileBuffer);


        if (buffer.length === 0) {
             return NextResponse.json({ error: "Empty file uploaded." }, { status: 400 });
        }
        
        // Since HTTPPOSTFILE doesn't send a filename header, we generate one.
        // We will try to guess the extension, but default to .dat
        // A more robust implementation might inspect magic bytes.
        // For this use case, we assume .wav is the most likely.
        const originalFilename = `upload-${Date.now()}.wav`;
        const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

        await fs.writeFile(finalFilePath, buffer);

        console.log(`[SERVER] Successfully received and saved file as ${originalFilename}`);
        
        return NextResponse.json({
            message: "File uploaded successfully.",
            filename: originalFilename
        }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
