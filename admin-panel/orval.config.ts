  export default {
  web: {
    input: 'http://127.0.0.1:8181/api/v1/swagger/doc.json',
    output: {
      mode: 'split',
      schemas: 'src/api/models',
      target: 'src/api/client',
      client: 'fetch',
      baseUrl: '/api/v1',  // relative path,
      override: {
        mutator: { path: 'src/api/fetch.ts', name: 'customFetch' },
        fetch: { includeHttpResponseReturnType: false },
      },
    },
  },
  };