
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import os from "os";

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
        // --- Compatibility Layer for different device headers ---
        
        let fileId: string | null = null;
        let chunkIndexStr: string | null = null;
        let totalChunksStr: string | null = null;
        let originalFilename: string | null = null;

        const userDataHeader = request.headers.get("x-userdata");

        if (userDataHeader) {
            // Standard parsing for browsers or other clients that use the combined header
            const data: Record<string, string> = {};
            const pairs = userDataHeader.split(';').map(s => s.trim());
            for (const pair of pairs) {
                const parts = pair.split(/:(.*)/s);
                if (parts.length === 2) {
                    const key = parts[0].trim().toLowerCase();
                    const value = parts[1].trim();
                    data[key] = value;
                }
            }
            fileId = data['x-file-id'];
            chunkIndexStr = data['x-chunk-index'];
            totalChunksStr = data['x-total-chunks'];
            originalFilename = data['x-original-filename'];
        } else {
            // Fallback for devices (like the provided Arduino code) sending individual headers
            chunkIndexStr = request.headers.get("x-chunk-index");
            totalChunksStr = request.headers.get("x-total-chunks");
            originalFilename = request.headers.get("x-original-filename");
            fileId = request.headers.get("x-file-id");
        }

        // --- End Compatibility Layer ---

        if (!chunkIndexStr || !totalChunksStr ) {
            return NextResponse.json({ error: "Missing one or more required chunking headers (x-chunk-index, x-total-chunks)." }, { status: 400 });
        }
        
        // If fileId or originalFilename are missing, create placeholders.
        // This ensures compatibility with the user's Arduino code.
        if (!fileId) {
            fileId = `device-upload-${Date.now()}`;
        }
        if (!originalFilename) {
            originalFilename = `${fileId}.dat`;
        }
        
        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }

        const chunkBuffer = await streamToBuffer(request.body);

        if (!chunkBuffer || chunkBuffer.byteLength === 0) {
            return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
        }
        
        const chunkIndex = parseInt(chunkIndexStr, 10);
        const totalChunks = parseInt(totalChunksStr, 10);
        
        const TMP_DIR = path.join(os.tmpdir(), "cellular-uploads");
        await fs.mkdir(TMP_DIR, { recursive: true });

        const safeFileId = fileId.replace(/[^a-zA-Z0-9-._]/g, '_');
        const chunkDir = path.join(TMP_DIR, safeFileId);
        await fs.mkdir(chunkDir, { recursive: true });

        const chunkFilePath = path.join(chunkDir, `${chunkIndex}.chunk`);
        await fs.writeFile(chunkFilePath, chunkBuffer);

        if ((chunkIndex + 1) === totalChunks) {
            const finalUploadDir = path.join(process.cwd(), "uploads");
            await fs.mkdir(finalUploadDir, { recursive: true });
            
            const safeOriginalFilename = path.basename(originalFilename).replace(/[^a-zA-Z0-9-._]/g, '_');
            const finalFilePath = path.join(finalUploadDir, safeOriginalFilename);
            
            const fileHandle = await fs.open(finalFilePath, 'w');
            for (let i = 0; i < totalChunks; i++) {
                const tempChunkPath = path.join(chunkDir, `${i}.chunk`);
                try {
                    const chunkData = await fs.readFile(tempChunkPath);
                    await fileHandle.write(chunkData);
                } catch (e) {
                     // A chunk is missing, abort assembly
                     await fileHandle.close();
                     await fs.unlink(finalFilePath); // Delete the incomplete file
                     await fs.rm(chunkDir, { recursive: true, force: true });
                     return NextResponse.json({ error: `Failed to assemble file, missing chunk ${i}.` }, { status: 500 });
                }
            }
            await fileHandle.close();
            
            await fs.rm(chunkDir, { recursive: true, force: true });
            
            return NextResponse.json({ message: "File upload complete.", filename: safeOriginalFilename }, { status: 200 });
        }

        return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
