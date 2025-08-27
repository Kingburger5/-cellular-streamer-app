
import { NextRequest, NextResponse } from "next/server";
import { format } from 'date-fns';
import { google } from 'googleapis';

async function logRequestToSheet(request: NextRequest) {
    const SPREADSHEET_ID = process.env.GOOGLE_SHEET_ID;
    const LOG_SHEET_NAME = "ConnectionLog"; // Log to a separate sheet
    const GOOGLE_SERVICE_ACCOUNT_CREDENTIALS = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;
     
    if (!SPREADSHEET_ID || !GOOGLE_SERVICE_ACCOUNT_CREDENTIALS) {
      console.log("Google Sheets env vars for logging not set. Skipping.");
      return;
    }

    try {
        const credentials = JSON.parse(GOOGLE_SERVICE_ACCOUNT_CREDENTIALS);
        const auth = new google.auth.GoogleAuth({
            credentials,
            scopes: ['https://www.googleapis.com/auth/spreadsheets'],
        });
        const sheets = google.sheets({ version: 'v4', auth });

        const timestamp = format(new Date(), "yyyy-MM-dd'T'HH:mm:ss.SSSxxx");
        const ip = request.ip ?? request.headers.get('x-forwarded-for') ?? 'N/A';
        const userAgent = request.headers.get('user-agent') || 'N/A';
        const contentType = request.headers.get('content-type') || 'N/A';
        
        const headers: string[] = [];
        request.headers.forEach((value, key) => {
            if (key.toLowerCase().startsWith('x-')) { // Log custom headers
                headers.push(`${key}: ${value}`);
            }
        });

        const newRow = [
            timestamp,
            ip,
            userAgent,
            contentType,
            headers.join('\n')
        ];
        
        await sheets.spreadsheets.values.append({
            spreadsheetId: SPREADSHEET_ID,
            range: `${LOG_SHEET_NAME}!A1`,
            valueInputOption: 'USER_ENTERED',
            requestBody: {
                values: [newRow],
            },
        });

    } catch (error) {
        console.error("Failed to write request log to Google Sheet:", error);
    }
}


/**
 * Handles file uploads. It does not save the file to disk.
 * Instead, it processes the file in memory.
 */
export async function POST(request: NextRequest) {
    
    // Log the request details to a Google Sheet instead of a local file
    await logRequestToSheet(request);

    try {
        if (!request.body) {
            return NextResponse.json({ error: "Empty request body." }, { status: 400 });
        }
        
        const fileBuffer = await request.arrayBuffer();

        if (fileBuffer.byteLength === 0) {
             return NextResponse.json({ error: "Empty file content." }, { status: 400 });
        }
        
        // Since we are not saving, we just confirm receipt.
        // The actual processing will now be initiated by the client after upload.
        const filename = request.headers.get('x-original-filename') || `upload-${Date.now()}.wav`;

        console.log(`[SERVER] Successfully received file in memory: ${filename}`);
        
        // We can return the buffer to the client if it needs to process it,
        // but for now, just confirming success is enough. The client-side uploader
        // will trigger the processing action.
        return NextResponse.json({
            message: "File uploaded successfully and is ready for processing.",
            filename: filename,
        }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
