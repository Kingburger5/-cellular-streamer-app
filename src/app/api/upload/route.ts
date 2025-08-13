
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// This helper function reads a ReadableStream into a Buffer.
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

export async function POST(request: NextRequest) {
    try {
        await fs.mkdir(UPLOAD_DIR, { recursive: true });

        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }

        const fileBuffer = await streamToBuffer(request.body);

        if (!fileBuffer || fileBuffer.byteLength === 0) {
            return NextResponse.json({ error: "Empty file content received." }, { status: 400 });
        }
        
        // Since the new Arduino code sends no metadata, we generate a unique filename on the server.
        const filename = `upload-${Date.now()}.dat`;
        const filePath = path.join(UPLOAD_DIR, filename);

        await fs.writeFile(filePath, fileBuffer);

        console.log(`Successfully received and saved file as ${filename}`);
        
        return NextResponse.json({ message: "File uploaded successfully.", filename: filename }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
