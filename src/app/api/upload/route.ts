
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// Global in-memory store for chunks.
// This will be shared across requests in the same server instance.
const chunkStore = new Map<string, Map<number, Buffer>>();

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

function parseUserDataHeader(header: string | null): Record<string, string> {
    if (!header) return {};
    const data: Record<string, string> = {};
    header.split(';').forEach(part => {
        const firstColonIndex = part.indexOf(':');
        if (firstColonIndex !== -1) {
            const key = part.substring(0, firstColonIndex).trim().toLowerCase();
            const value = part.substring(firstColonIndex + 1).trim();
            data[key] = value;
        }
    });
    return data;
}

export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();

    const userDataHeader = request.headers.get("x-userdata");
    const parsedHeaders = parseUserDataHeader(userDataHeader);

    const fileIdentifier = parsedHeaders["x-file-id"];
    const chunkIndexStr = parsedHeaders["x-chunk-index"];
    const totalChunksStr = parsedHeaders["x-total-chunks"];
    const originalFilenameUnsafe = parsedHeaders["x-original-filename"];
    
    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        console.error("[SERVER] FATAL: Missing required fields in parsed x-userdata header.", { parsedHeaders });
        return NextResponse.json({ error: "Could not parse required fields from x-userdata header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    const originalFilename = path.basename(originalFilenameUnsafe);
    const safeIdentifier = path.basename(fileIdentifier);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);

    // Get or create the chunk map for this file identifier
    if (!chunkStore.has(safeIdentifier)) {
      chunkStore.set(safeIdentifier, new Map<number, Buffer>());
    }
    const fileChunks = chunkStore.get(safeIdentifier)!;

    // Store the chunk
    fileChunks.set(chunkIndex, buffer);

    console.log(`[SERVER] Stored chunk ${chunkIndex + 1}/${totalChunks} for ${originalFilename} in memory.`);

    // If all chunks have been received, assemble the file
    if (fileChunks.size === totalChunks) {
      console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling...`);
      
      const chunkArray: Buffer[] = [];
      for (let i = 0; i < totalChunks; i++) {
        if (!fileChunks.has(i)) {
          console.error(`[SERVER] FATAL: Missing chunk ${i} for ${originalFilename}. Aborting assembly.`);
          chunkStore.delete(safeIdentifier); // Cleanup
          return NextResponse.json({ error: `Missing chunk ${i} during assembly.` }, { status: 500 });
        }
        chunkArray.push(fileChunks.get(i)!);
      }

      const finalFileBuffer = Buffer.concat(chunkArray);
      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

      await fs.writeFile(finalFilePath, finalFileBuffer);
      
      console.log(`[SERVER] Successfully assembled and saved ${originalFilename} to ${finalFilePath}.`);
      
      // Clean up the memory for this file
      chunkStore.delete(safeIdentifier);
      console.log(`[SERVER] Cleaned up in-memory store for ${safeIdentifier}`);

      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
