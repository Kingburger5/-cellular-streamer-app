
import { NextRequest, NextResponse } from "next/server";
import { adminApp } from "@/lib/firebase-admin";
import { getStorage } from "firebase-admin/storage";
import { PassThrough } from "stream";

const bucket = getStorage(adminApp).bucket();

function parseUserDataHeader(header: string): Record<string, string> {
    const result: Record<string, string> = {};
    if (!header) return result;

    const pairs = header.split(';');
    pairs.forEach(pair => {
        const separatorIndex = pair.indexOf(':');
        if (separatorIndex > 0) {
            const key = pair.substring(0, separatorIndex).trim().toLowerCase();
            const value = pair.substring(separatorIndex + 1).trim();
            result[key] = value;
        }
    });
    return result;
}

export async function POST(request: NextRequest) {
  try {
    console.log("[SERVER] Received upload request.");

    let userData = null;
    let foundHeaderKey = null;

    console.log("[SERVER] All incoming headers:");
    request.headers.forEach((value, key) => {
        console.log(`- ${key}: ${value}`);
        if (key.toLowerCase().includes('userdata') || value.includes('X-File-ID')) {
            userData = value;
            foundHeaderKey = key;
        }
    });
    
    if (userData) {
         console.log(`[SERVER] Found custom header string in key '${foundHeaderKey}': ${userData}`);
    } else {
        userData = request.headers.get("x-userdata");
        if(userData) console.log("[SERVER] Found custom header in 'x-userdata'");
    }

    if (!userData) {
      return NextResponse.json({ error: "Missing USERDATA or equivalent custom header." }, { status: 400 });
    }
    
    const headers = parseUserDataHeader(userData);
    
    const fileIdentifier = headers["x-file-id"];
    const chunkIndexStr = headers["x-chunk-index"];
    const totalChunksStr = headers["x-total-chunks"];
    const originalFilename = headers["x-original-filename"]?.replace(/^\//, ''); // Remove leading slash

    console.log(`[SERVER] Parsed Headers: x-file-id=${fileIdentifier}, x-chunk-index=${chunkIndexStr}, x-total-chunks=${totalChunksStr}, x-original-filename=${originalFilename}`);

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilename) {
      console.error("[SERVER] Missing required fields in parsed USERDATA header.");
      return NextResponse.json({ error: "Missing required fields in parsed USERDATA header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      console.error("[SERVER] Empty chunk received.");
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    console.log(`[SERVER] Received chunk ${chunkIndex + 1}/${totalChunks} with size ${buffer.length} bytes for ${originalFilename}.`);

    const file = bucket.file(originalFilename);

    if (chunkIndex === 0) {
        // This is the first chunk, we start a new upload stream
        console.log(`[SERVER] Starting new upload for ${originalFilename}.`);
        const stream = file.createWriteStream({
            metadata: {
                contentType: 'application/octet-stream',
            },
            resumable: false,
        });
        stream.write(buffer);
        // For the first chunk, we just write and wait for the next
    } else {
        // For subsequent chunks, we need to append. This is tricky with GCS.
        // The most robust way is to download existing chunks, append new one, and re-upload.
        // A simpler, though less efficient method, is to use compose for many small chunks,
        // but for now, we'll try a direct append approach which might have issues with some file types.
        // A truly robust solution would require temporary local storage or a more complex GCS compose strategy.
        
        // Let's try appending via a stream. This assumes the file exists from chunk 0.
        const passthroughStream = new PassThrough();
        passthroughStream.write(buffer);
        passthroughStream.end();

        const [existingFile] = await bucket.file(originalFilename).get();
        const existingStream = existingFile.createReadStream();
        
        const newFileStream = bucket.file(originalFilename).createWriteStream();
        
        existingStream.pipe(newFileStream, { end: false });
        existingStream.on('end', () => {
            passthroughStream.pipe(newFileStream);
        });
    }

    if (chunkIndex === totalChunks - 1) {
      console.log(`[SERVER] Final chunk received for ${originalFilename}. Upload should be complete.`);
      return NextResponse.json({ message: "File uploaded successfully.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
