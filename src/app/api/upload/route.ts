
import { NextRequest, NextResponse } from "next/server";
import { adminApp } from "@/lib/firebase-admin";
import { PassThrough } from "stream";

const bucket = adminApp.storage().bucket();

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

    // Create a write stream to the file in Firebase Storage.
    // This will either create the file or append to it if it already exists.
    // For chunked uploads, we rely on the client to send chunks in order.
    // GCS doesn't have a simple "append" so we manage this by streaming.
    const stream = file.createWriteStream({
        metadata: {
            contentType: 'application/octet-stream',
        },
        // We can't make this resumable in the same way as a single upload,
        // so we manage chunks ourselves.
        resumable: false, 
    });

    const passthrough = new PassThrough();
    passthrough.write(buffer);
    passthrough.end();

    // The logic for appending is simplified here. A robust implementation
    // for parallel uploads would require composing parts. For serial chunks,
    // we can manage the stream. On chunk 0, we start new. On others, we would
    // ideally append. GCS write streams overwrite by default.
    // The correct way to handle this is to write chunks to temporary files
    // and then compose them at the end.

    // Let's try a simplified approach first. If it is chunk 0, just write.
    // If it's a subsequent chunk, we have an issue because we can't easily append.
    // Let's adjust the logic to handle this. We will have to write to temporary
    // chunk files in storage and then compose them.

    const tempChunkFile = bucket.file(`tmp/${originalFilename}.${chunkIndex}`);
    await tempChunkFile.save(buffer);
    console.log(`[SERVER] Saved chunk ${chunkIndex} to temporary file.`);

    if (chunkIndex === totalChunks - 1) {
        console.log(`[SERVER] Final chunk received for ${originalFilename}. Composing file...`);
        
        const tempChunkFiles = [];
        for (let i = 0; i < totalChunks; i++) {
            tempChunkFiles.push(bucket.file(`tmp/${originalFilename}.${i}`));
        }

        // Check if all chunks exist before composing
        const allChunksExist = await Promise.all(tempChunkFiles.map(f => f.exists()));
        if(allChunksExist.some(e => !e[0])) {
             console.error("[SERVER] Missing some temporary chunks, cannot compose file.");
             return NextResponse.json({ error: "Missing some chunks, cannot compose file." }, { status: 500 });
        }
        
        await bucket.combine(tempChunkFiles, bucket.file(originalFilename));
        console.log(`[SERVER] Successfully composed ${originalFilename}.`);

        // Clean up temporary chunk files
        await Promise.all(tempChunkFiles.map(f => f.delete()));
        console.log(`[SERVER] Deleted temporary chunk files for ${originalFilename}.`);

        return NextResponse.json({ message: "File uploaded and composed successfully.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received and stored.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
