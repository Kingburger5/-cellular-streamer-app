
import { NextRequest, NextResponse } from "next/server";
import { getAdminStorage } from "@/lib/firebase-admin";

const BUCKET_NAME = "cellular-data-streamer.firebasestorage.app";

/**
 * Acts as a secure relay for file uploads from SIM7600 modules.
 * It accepts the raw file in the request body and uses the Firebase Admin SDK
 * to save it to a private Firebase Storage bucket.
 * The original filename is expected in the 'x-original-filename' header.
 */
export async function POST(request: NextRequest) {
    try {
        // Get the raw file data from the request body.
        const fileBuffer = await request.arrayBuffer();

        if (!fileBuffer || fileBuffer.byteLength === 0) {
            console.warn("[SERVER] Upload attempt with empty file rejected.");
            return NextResponse.json({ error: "File content is empty." }, { status: 400 });
        }

        // The cellular module should provide the intended filename in this header.
        const originalFilename = request.headers.get('x-original-filename');
        if (!originalFilename) {
            console.warn("[SERVER] Upload attempt without 'x-original-filename' header rejected.");
            return NextResponse.json({ error: "Missing 'x-original-filename' header." }, { status: 400 });
        }

        const filePath = `uploads/${originalFilename}`;
        const contentType = request.headers.get('content-type') || 'application/octet-stream';

        // Use the Admin SDK to upload the file buffer to the bucket.
        const adminStorage = await getAdminStorage();
        const bucket = adminStorage.bucket(BUCKET_NAME);
        const file = bucket.file(filePath);

        await file.save(Buffer.from(fileBuffer), {
            metadata: { contentType }
        });

        console.log(`[SERVER] Relay upload successful. Saved '${originalFilename}' to Firebase Storage.`);

        // Notify any connected clients that a new file has arrived.
        const channel = new BroadcastChannel('new-upload');
        channel.postMessage({ filename: originalFilename });
        channel.close();

        return NextResponse.json({
            message: "File uploaded successfully via relay.",
            filename: originalFilename,
        }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload relay handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown server error occurred.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
