
'use server';
/**
 * @fileOverview A flow for appending data to a Google Sheet.
 */

import { ai } from '@/ai/genkit';
import { z } from 'genkit';
import { google } from 'googleapis';
import type { DataPoint } from '@/lib/types';

// Define the schema for the flow's input
const AppendToSheetInputSchema = z.object({
  dataPoint: z.custom<DataPoint>(),
  originalFilename: z.string(),
});
export type AppendToSheetInput = z.infer<typeof AppendToSheetInputSchema>;

// This async wrapper is the function that will be called from other server-side code.
export async function appendToSheet(input: AppendToSheetInput): Promise<string> {
    return appendToSheetFlow(input);
}


// The Genkit flow is defined here but NOT exported directly.
const appendToSheetFlow = ai.defineFlow(
  {
    name: 'appendToSheetFlow',
    inputSchema: AppendToSheetInputSchema,
    outputSchema: z.string(), // Returns a success message or error
  },
  async ({ dataPoint, originalFilename }) => {
    // Environment variables must be set for this to work.
    const SPREADSHEET_ID = process.env.GOOGLE_SHEET_ID;
    const SHEET_NAME = process.env.GOOGLE_SHEET_NAME;
    const GOOGLE_SERVICE_ACCOUNT_CREDENTIALS = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;

    if (!SPREADSHEET_ID || !SHEET_NAME || !GOOGLE_SERVICE_ACCOUNT_CREDENTIALS) {
      const errorMsg = "Google Sheets environment variables are not set. Skipping sheet append.";
      console.error(errorMsg);
      return errorMsg;
    }

    try {
        const credentials = JSON.parse(GOOGLE_SERVICE_ACCOUNT_CREDENTIALS);

        const auth = new google.auth.GoogleAuth({
            credentials,
            scopes: ['https://www.googleapis.com/auth/spreadsheets'],
        });
        
        const sheets = google.sheets({ version: 'v4', auth });
        
        // Map the DataPoint to the order of columns in the spreadsheet
        const newRow = [
            '', // ID - let sheets handle it or leave blank
            dataPoint.siteName || '',
            dataPoint.surveyDate || '',
            dataPoint.surveyFinishTime || '',
            dataPoint.make || '',
            dataPoint.model || '',
            dataPoint.serial || '',
            dataPoint.temperature ? dataPoint.temperature.toString() : '',
            '', // File Type - can be derived from filename if needed
            originalFilename,
            new Date().toISOString(), // Date Added
        ];

        await sheets.spreadsheets.values.append({
            spreadsheetId: SPREADSHEET_ID,
            range: `${SHEET_NAME}!A1`, // Append after the last row of the table
            valueInputOption: 'USER_ENTERED',
            requestBody: {
                values: [newRow],
            },
        });

        const successMsg = `Successfully appended data for ${originalFilename} to Google Sheet.`;
        console.log(successMsg);
        return successMsg;

    } catch (error) {
       let errorMessage = 'An unknown error occurred while appending to the Google Sheet.';
       if (error instanceof Error) {
            errorMessage = `Error appending to Google Sheet: ${error.message}`;
       }
       console.error(errorMessage, error);
       return errorMessage;
    }
  }
);
