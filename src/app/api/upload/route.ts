
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
const TMP_DIR = path.join(process.cwd(), "tmp");

// Ensures a directory exists.
async function ensureDirExists(dir: string) {
  try {
    await fs.mkdir(dir, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error(`[SERVER] Error creating directory ${dir}:`, error);
      throw new Error(`Could not create directory ${dir}.`);
    }
  }
}

// Parses the custom user-data header string
function parseUserDataHeader(header: string): Record<string, string> {
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
    console.log("\n--- [SERVER] New POST request received ---");
    await ensureDirExists(UPLOAD_DIR);
    await ensureDirExists(TMP_DIR);

    // All headers are lowercased by Next.js
    const userDataHeader = request.headers.get("x-userdata");

    if (!userDataHeader) {
      console.error("[SERVER] FATAL: Missing 'x-userdata' header.");
      return NextResponse.json({ error: "Missing required x-userdata header." }, { status: 400 });
    }
    
    console.log(`[SERVER] Raw x-userdata header: "${userDataHeader}"`);
    const parsedHeaders = parseUserDataHeader(userDataHeader);
    console.log("[SERVER] Parsed userdata object:", parsedHeaders);

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
    const safeIdentifier = path.basename(fileIdentifier);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        console.error("[SERVER] Empty chunk received.");
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    
    // Write chunk to a temporary file
    const chunkFilePath = path.join(TMP_DIR, `${safeIdentifier}.part_${chunkIndex}`);
    await fs.writeFile(chunkFilePath, buffer);

    console.log(`[SERVER] Stored chunk ${chunkIndex + 1}/${totalChunks} for ${originalFilename} to ${chunkFilePath}`);

    if ((chunkIndex + 1) === totalChunks) {
      console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
      
      const chunkPaths = [];
      for (let i = 0; i < totalChunks; i++) {
        chunkPaths.push(path.join(TMP_DIR, `${safeIdentifier}.part_${i}`));
      }

      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);
      const fileHandle = await fs.open(finalFilePath, 'w');

      for (const partPath of chunkPaths) {
        try {
            const chunkContent = await fs.readFile(partPath);
            await fileHandle.write(chunkContent);
        } catch (readError) {
             console.error(`[SERVER] Error reading chunk ${partPath}, maybe it hasn't arrived yet. Retrying in a moment.`, readError);
             // Simple retry logic
             await new Promise(resolve => setTimeout(resolve, 500));
             try {
                const chunkContent = await fs.readFile(partPath);
                await fileHandle.write(chunkContent);
             } catch (retryError) {
                 console.error(`[SERVER] FATAL: Could not read chunk ${partPath} on retry. Aborting assembly.`, retryError);
                 await fileHandle.close();
                 // Don't delete chunks here so we can debug them
                 return NextResponse.json({ error: `Failed to assemble file, chunk ${path.basename(partPath)} missing or unreadable.` }, { status: 500 });
             }
        }
      }

      await fileHandle.close();
      console.log(`[SERVER] Successfully assembled and saved ${originalFilename} to ${finalFilePath}.`);

      // Asynchronously clean up temporary chunk files
      Promise.all(chunkPaths.map(p => fs.unlink(p)))
        .then(() => console.log(`[SERVER] Cleaned up temporary chunks for ${safeIdentifier}`))
        .catch(cleanupErr => console.error(`[SERVER] Error during chunk cleanup for ${safeIdentifier}:`, cleanupErr));

      return NextResponse.json({ message: "File upload complete. Processing.", filename: originalFilename }, { status: 200 });
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

