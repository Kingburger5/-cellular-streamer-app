
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// Helper function to read a ReadableStream into a Buffer.
async function streamToBuffer(stream: ReadableStream<Uint8Array>): Promise<Buffer> {
    const reader = stream.getReader();
    const chunks: Uint8Array[] = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) {
            break;
        }
        chunks.push(value);
    }
    return Buffer.concat(chunks);
}

/**
 * Handles file uploads from devices using simple POST requests,
 * such as the SIM7600's AT+HTTPPOSTFILE command.
 *
 * This handler is not designed for chunked uploads. It treats each POST
 * request as a complete file.
 */
export async function POST(request: NextRequest) {
    try {
        await fs.mkdir(UPLOAD_DIR, { recursive: true });

        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }
        
        const fileBuffer = await streamToBuffer(request.body);

        if (fileBuffer.length === 0) {
             return NextResponse.json({ error: "Empty file uploaded." }, { status: 400 });
        }
        
        // Since HTTPPOSTFILE doesn't send a filename header, we generate one.
        // We will try to guess the extension, but default to .dat
        // A more robust implementation might inspect magic bytes.
        // For this use case, we assume .wav is the most likely.
        const originalFilename = `upload-${Date.now()}.wav`;
        const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

        await fs.writeFile(finalFilePath, fileBuffer);

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
