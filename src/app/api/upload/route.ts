
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import { Writable } from "stream";

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

// Parses the composite x-userdata header from the Arduino device
function parseUserDataHeader(header: string | null): Record<string, string> {
    const metadata: Record<string, string> = {};
    if (!header) {
        return metadata;
    }

    header.split(';').forEach(part => {
        const firstColonIndex = part.indexOf(':');
        if (firstColonIndex > -1) {
            const key = part.substring(0, firstColonIndex).trim().toLowerCase();
            const value = part.substring(firstColonIndex + 1).trim();
            metadata[key] = value;
        }
    });

    return metadata;
}


export async function POST(request: NextRequest) {
    try {
        await fs.mkdir(UPLOAD_DIR, { recursive: true });

        const userData = parseUserDataHeader(request.headers.get("x-userdata"));

        const fileId = userData['x-file-id'];
        const chunkIndexStr = userData['x-chunk-index'];
        const totalChunksStr = userData['x-total-chunks'];
        const originalFilename = userData['x-original-filename'];

        if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilename) {
            const missing = [
                !fileId && "x-file-id",
                !chunkIndexStr && "x-chunk-index",
                !totalChunksStr && "x-total-chunks",
                !originalFilename && "x-original-filename"
            ].filter(Boolean).join(", ");
            return NextResponse.json({ error: `Missing one or more required fields in x-userdata header. Missing: ${missing}` }, { status: 400 });
        }

        const chunkIndex = parseInt(chunkIndexStr, 10);
        const totalChunks = parseInt(totalChunksStr, 10);

        if (isNaN(chunkIndex) || isNaN(totalChunks)) {
             return NextResponse.json({ error: "Invalid chunk index or total chunks number." }, { status: 400 });
        }
        
        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }
        
        const chunkBuffer = await streamToBuffer(request.body);
        const tempChunkPath = path.join(UPLOAD_DIR, `${fileId}.part_${chunkIndex}`);
        await fs.writeFile(tempChunkPath, chunkBuffer);
        
        console.log(`[SERVER] Received chunk ${chunkIndex + 1}/${totalChunks} for ${fileId}`);
        
        // Check if all chunks have been uploaded
        const chunkFiles = (await fs.readdir(UPLOAD_DIR)).filter(
            (file) => file.startsWith(`${fileId}.part_`)
        );

        if (chunkFiles.length === totalChunks) {
            console.log(`[SERVER] All ${totalChunks} chunks received for ${fileId}. Reassembling file.`);
            
            const finalFilePath = path.join(UPLOAD_DIR, originalFilename);
            const writeStream = fs.createWriteStream(finalFilePath);

            for (let i = 0; i < totalChunks; i++) {
                const chunkPath = path.join(UPLOAD_DIR, `${fileId}.part_${i}`);
                const chunkBuffer = await fs.readFile(chunkPath);
                writeStream.write(chunkBuffer);
                await fs.unlink(chunkPath); // Clean up chunk
            }
            writeStream.end();

            console.log(`[SERVER] Successfully reassembled and saved file as ${originalFilename}`);
            return NextResponse.json({ message: "File uploaded and reassembled successfully.", filename: originalFilename }, { status: 200 });
        }

        return NextResponse.json({ message: "Chunk received successfully." }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
