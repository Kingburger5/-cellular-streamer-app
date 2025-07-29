
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import { storage, database } from "@/lib/firebase";
import { ref as dbRef, set, remove } from "firebase/database";
import { 
  ref as storageRef, 
  uploadBytes, 
  getBytes, 
  listAll, 
  deleteObject 
} from "firebase/storage";


const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// Helper to ensure upload directory exists
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
    const data: Record<string, string> = {};
    if (!header) return data;

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

// Function to update status in Firebase Realtime Database
async function updateStatus(identifier: string, status: string, error: string | null = null) {
    const safeIdentifier = identifier.replace(/[.#$[\]]/g, '_');
    const statusRef = dbRef(database, `uploads/${safeIdentifier}`);
    await set(statusRef, { 
        status, 
        error, 
        lastUpdated: new Date().toISOString() 
    });
}

export async function POST(request: NextRequest) {
  let fileIdentifier = "unknown";
  try {
    await ensureUploadDirExists();

    const userDataHeader = request.headers.get("x-userdata");
    const parsedHeaders = parseUserDataHeader(userDataHeader);

    fileIdentifier = parsedHeaders["x-file-id"];
    const chunkIndexStr = parsedHeaders["x-chunk-index"];
    const totalChunksStr = parsedHeaders["x-total-chunks"];
    const originalFilenameUnsafe = parsedHeaders["x-original-filename"];
    
    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
      const errorMsg = "Missing required fields in parsed x-userdata header.";
      await updateStatus(fileIdentifier || 'unknown_request', 'Error', errorMsg);
      return NextResponse.json({ error: errorMsg }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    const originalFilename = path.basename(originalFilenameUnsafe);
    const safeIdentifier = fileIdentifier.replace(/[.#$[\]/]/g, '_');
    
    await updateStatus(safeIdentifier, `Receiving chunk ${chunkIndex + 1}/${totalChunks}`);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      const errorMsg = "Empty chunk received.";
      await updateStatus(safeIdentifier, 'Error', errorMsg);
      return NextResponse.json({ error: errorMsg }, { status: 400 });
    }

    // Upload chunk to Firebase Storage
    const chunkPath = `tmp/${safeIdentifier}/${chunkIndex}.chunk`;
    const chunkStorageRef = storageRef(storage, chunkPath);
    await uploadBytes(chunkStorageRef, chunkBuffer);

    await updateStatus(safeIdentifier, `Stored chunk ${chunkIndex + 1}/${totalChunks} to cloud`);

    // If all chunks have been received, assemble the file
    if (chunkIndex + 1 === totalChunks) {
      await updateStatus(safeIdentifier, `All chunks received. Assembling file...`);
      
      const chunkDirRef = storageRef(storage, `tmp/${safeIdentifier}`);
      const chunkArray = new Array(totalChunks);

      for(let i=0; i<totalChunks; i++) {
        const chunkRef = storageRef(storage, `tmp/${safeIdentifier}/${i}.chunk`);
        try {
            const bytes = await getBytes(chunkRef);
            chunkArray[i] = Buffer.from(bytes);
        } catch(e) {
             const errorMsg = `FATAL: Could not get chunk ${i} from cloud storage.`;
             await updateStatus(safeIdentifier, 'Error', errorMsg);
             console.error(errorMsg, e);
             return NextResponse.json({ error: errorMsg }, { status: 500 });
        }
      }

      const finalFileBuffer = Buffer.concat(chunkArray);
      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

      await fs.writeFile(finalFilePath, finalFileBuffer);
      
      await updateStatus(safeIdentifier, `File assembled successfully. Cleaning up...`);

      // Clean up temporary chunks from Firebase Storage
      const listResult = await listAll(chunkDirRef);
      await Promise.all(listResult.items.map(item => deleteObject(item)));

      // Final status update and cleanup from RTDB
      await updateStatus(safeIdentifier, `Upload complete: ${originalFilename}`);
      setTimeout(() => remove(dbRef(database, `uploads/${safeIdentifier}`)), 30000); // cleanup RTDB entry after 30s

      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    const safeId = fileIdentifier.replace(/[.#$[\]/]/g, '_');
    await updateStatus(safeId, 'Error', errorMessage);
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
