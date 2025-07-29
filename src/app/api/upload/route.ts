
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// A simple in-memory store for chunks.
const chunkStore = new Map<string, Buffer[]>();

// Ensures the upload directory exists.
async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("[SERVER] Error creating upload directory:", error);
      throw new Error("Could not create upload directory.");
    }
  }
}

export async function POST(request: NextRequest) {
  try {
    console.log("\n--- [SERVER] New POST request received ---");
    await ensureUploadDirExists();

    const allHeaders = Object.fromEntries(request.headers.entries());
    console.log("[SERVER] All Request Headers:", allHeaders);
    
    // All headers are lowercased by Next.js
    const userDataHeader = request.headers.get("x-userdata");

    if (!userDataHeader) {
      console.error("[SERVER] FATAL: Missing 'x-userdata' header.");
      return NextResponse.json({ error: "Missing required x-userdata header." }, { status: 400 });
    }
    
    console.log(`[SERVER] Raw x-userdata header: "${userDataHeader}"`);

    // Robustly parse the userdata header string
    const parsedHeaders: Record<string, string> = {};
    userDataHeader.split(';').forEach(part => {
        const firstColonIndex = part.indexOf(':');
        if (firstColonIndex !== -1) {
            const key = part.substring(0, firstColonIndex).trim().toLowerCase();
            const value = part.substring(firstColonIndex + 1).trim();
            parsedHeaders[key] = value;
        }
    });

    console.log("[SERVER] Parsed userdata object:", parsedHeaders);

    // Access keys in lowercase
    const fileIdentifier = parsedHeaders["x-file-id"];
    const chunkIndexStr = parsedHeaders["x-chunk-index"];
    const totalChunksStr = parsedHeaders["x-total-chunks"];
    const originalFilenameUnsafe = parsedHeaders["x-original-filename"];
    
    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        const error = "[SERVER] FATAL: Missing one or more required fields in parsed x-userdata header.";
        console.error(error, { parsedHeaders });
        return NextResponse.json({ error: "Could not parse required fields from x-userdata header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    const originalFilename = path.basename(originalFilenameUnsafe);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        console.error("[SERVER] Empty chunk received.");
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    
    if (!chunkStore.has(fileIdentifier)) {
        chunkStore.set(fileIdentifier, []);
    }

    const chunks = chunkStore.get(fileIdentifier)!;
    chunks.push(buffer);

    console.log(`[SERVER] Stored chunk ${chunks.length}/${totalChunks} for ${originalFilename} (ID: ${fileIdentifier})`);

    if (chunks.length === totalChunks) {
      console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
      const fullFileBuffer = Buffer.concat(chunks);
      
      const safeFilename = path.basename(originalFilename);
      const finalFilePath = path.join(UPLOAD_DIR, safeFilename);

      // Asynchronously write file and don't await it to send response faster
      fs.writeFile(finalFilePath, fullFileBuffer).then(() => {
          console.log(`[SERVER] Successfully saved ${safeFilename} to ${finalFilePath}.`);
          chunkStore.delete(fileIdentifier);
      }).catch(err => {
          console.error(`[SERVER] Error writing final file ${safeFilename}:`, err);
          chunkStore.delete(fileIdentifier);
      });

      return NextResponse.json({ message: "File upload complete. Processing.", filename: safeFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunks.length}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
