
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
        const userDataHeader = request.headers.get("x-userdata");

        if (!userDataHeader) {
            return NextResponse.json({ error: "Missing x-userdata header." }, { status: 400 });
        }
        
        // This is a simple, robust parser for the specific format sent by the device.
        const data: Record<string, string> = {};
        const pairs = userDataHeader.split(';').map(s => s.trim());
        for (const pair of pairs) {
            const parts = pair.split(/:(.*)/s);
            if (parts.length === 2) {
                // Lowercase the key to handle case-insensitivity (e.g., X-File-ID vs x-file-id)
                const key = parts[0].trim().toLowerCase();
                const value = parts[1].trim();
                data[key] = value;
            }
        }

        const fileId = data['x-file-id'];
        const chunkIndexStr = data['x-chunk-index'];
        const totalChunksStr = data['x-total-chunks'];
        const originalFilename = data['x-original-filename'];

        if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilename) {
            return NextResponse.json({ error: "Missing one or more required fields in x-userdata header." }, { status: 400 });
        }
        
        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }

        // Use the robust stream-to-buffer method to handle binary data correctly.
        const chunkBuffer = await streamToBuffer(request.body);

        if (!chunkBuffer || chunkBuffer.byteLength === 0) {
            return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
        }
        
        const chunkIndex = parseInt(chunkIndexStr, 10);
        const totalChunks = parseInt(totalChunksStr, 10);
        
        // Use the OS's temporary directory, which is guaranteed to be writable.
        const TMP_DIR = path.join(os.tmpdir(), "cellular-uploads");
        await fs.mkdir(TMP_DIR, { recursive: true });

        // Sanitize the fileId to ensure it's a valid directory name.
        const safeFileId = fileId.replace(/[^a-zA-Z0-9-._]/g, '_');
        const chunkDir = path.join(TMP_DIR, safeFileId);
        await fs.mkdir(chunkDir, { recursive: true });

        const chunkFilePath = path.join(chunkDir, `${chunkIndex}.chunk`);
        await fs.writeFile(chunkFilePath, chunkBuffer);

        if ((chunkIndex + 1) === totalChunks) {
            const finalUploadDir = path.join(process.cwd(), "uploads");
            await fs.mkdir(finalUploadDir, { recursive: true });
            
            // Sanitize final filename to prevent path traversal attacks
            const safeOriginalFilename = path.basename(originalFilename).replace(/[^a-zA-Z0-9-._]/g, '_');
            const finalFilePath = path.join(finalUploadDir, safeOriginalFilename);
            
            const fileHandle = await fs.open(finalFilePath, 'w');
            for (let i = 0; i < totalChunks; i++) {
                const tempChunkPath = path.join(chunkDir, `${i}.chunk`);
                const chunkData = await fs.readFile(tempChunkPath);
                await fileHandle.write(chunkData);
            }
            await fileHandle.close();
            
            await fs.rm(chunkDir, { recursive: true, force: true });
            
            return NextResponse.json({ message: "File upload complete.", filename: safeOriginalFilename }, { status: 200 });
        }

        return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error.stack);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
