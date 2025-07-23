
"use client";

import { useState, useEffect } from 'react';

interface FormattedDateProps {
    date: Date;
}

export function FormattedDate({ date }: FormattedDateProps) {
  const [formattedDate, setFormattedDate] = useState<string>('');

  useEffect(() => {
    setFormattedDate(new Date(date).toLocaleString("en-US", {
        year: 'numeric',
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: true
    }));
  }, [date]);

  return <span>{formattedDate}</span>;
}
