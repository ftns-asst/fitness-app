import {customFetch} from "./fetch.ts";
import {setToken} from "./token.ts";

export const loginAdmin = async (password: string): Promise<string> => {
  const token = await customFetch<string>("/api/v1/admin/auth", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({password}),
  });
  setToken(token);
  return token;
}
