# Create Comprehensive REST API Documentation

## Description
We need comprehensive documentation for all FCEUX REST API endpoints so that developers (including AI assistants) can easily understand and use the API. The documentation should be machine-readable and human-friendly, with clear examples and response formats.

## Current State
- Multiple REST API endpoints have been implemented across several PRs
- No centralized documentation exists
- Endpoint discovery requires reading source code
- No standardized format for request/response examples

## Proposed Documentation Structure

### 1. API Overview Document (`docs/REST_API.md`)
- Introduction to FCEUX REST API
- Base URL and port configuration
- Authentication (if any)
- Common response formats
- Error handling patterns
- Rate limiting (if applicable)

### 2. Endpoint Reference (`docs/api/`)
Organized by category with detailed documentation for each endpoint:

#### System Endpoints (`docs/api/system.md`)
- GET /api/system/info
- GET /api/system/ping  
- GET /api/system/capabilities

#### Emulation Control (`docs/api/emulation.md`)
- POST /api/emulation/pause
- POST /api/emulation/resume
- GET /api/emulation/status

#### ROM Information (`docs/api/rom.md`)
- GET /api/rom/info

#### Memory Access (`docs/api/memory.md`)
- GET /api/memory/{address}
- GET /api/memory/range/{start}/{length}
- POST /api/memory/range/{start}
- POST /api/memory/batch (if implemented)

#### Input Control (`docs/api/input.md`)
- GET /api/input/status
- POST /api/input/port/{port}/press
- POST /api/input/port/{port}/release
- POST /api/input/port/{port}/state

#### Media Operations (`docs/api/media.md`)
- POST /api/screenshot
- GET /api/screenshot/last
- POST /api/savestate
- POST /api/loadstate
- GET /api/savestate/list

### 3. OpenAPI/Swagger Specification (`docs/api/openapi.yaml`)
Machine-readable API specification that can be used to:
- Generate client libraries
- Create interactive documentation
- Enable API testing tools
- Support AI assistants in understanding the API

### 4. Quick Start Guide (`docs/api/quickstart.md`)
- Prerequisites
- Starting the REST API server
- Basic examples with curl
- Common use cases
- Troubleshooting

## Documentation Format for Each Endpoint

Each endpoint should document:

```markdown
### {METHOD} {PATH}

**Description**: Brief description of what the endpoint does

**Parameters**:
- Path parameters (if any)
- Query parameters (if any)
- Request body schema (for POST/PUT)

**Request Example**:
```bash
curl -X {METHOD} http://localhost:8080{PATH} \
  -H "Content-Type: application/json" \
  -d '{request_body}'
```

**Response**:
- Status codes and their meanings
- Response schema
- Example responses for success and error cases

**Error Responses**:
- Common error scenarios
- Error response format

**Notes**:
- Any special considerations
- Performance characteristics
- Limitations
```

## Example Documentation Entry

```markdown
### GET /api/memory/range/{start}/{length}

**Description**: Read a range of bytes from NES memory

**Parameters**:
- `start` (path): Starting address in hex format (e.g., 0x0000)
- `length` (path): Number of bytes to read (max 4096)

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/memory/range/0x0000/256
```

**Response**:
```json
{
  "start": "0x0000",
  "length": 256,
  "data": "base64_encoded_data",
  "hex": "first_64_bytes_in_hex",
  "checksum": "0x00"
}
```

**Error Responses**:
- `400 Bad Request`: Invalid address format or length exceeds maximum
- `503 Service Unavailable`: No game loaded
- `504 Gateway Timeout`: Command execution timeout

**Notes**:
- Maximum read length is 4096 bytes
- Address must be in range 0x0000-0xFFFF
- ~332x faster than individual byte reads
```

## Implementation Tasks

1. **Create documentation structure**
   - Set up docs/api/ directory
   - Create template for endpoint documentation

2. **Document existing endpoints**
   - Extract information from source code
   - Test each endpoint to verify behavior
   - Document request/response formats

3. **Create OpenAPI specification**
   - Define schemas for all data types
   - Include all endpoints with parameters
   - Add example requests/responses

4. **Add inline code documentation**
   - Update source files with better comments
   - Link to documentation from code

5. **Create automated documentation generation**
   - Consider tools like Doxygen or similar
   - Generate docs from code comments
   - Keep documentation in sync with code

## Benefits

1. **Developer Experience**: Easy onboarding for new contributors
2. **AI Integration**: Machine-readable docs enable AI assistants to use the API effectively
3. **Testing**: Clear documentation enables better test coverage
4. **Client Development**: Enables creation of client libraries in various languages
5. **API Evolution**: Documents current state for future improvements

## Success Criteria

- [ ] All endpoints documented with examples
- [ ] OpenAPI specification validates correctly
- [ ] Documentation is easily discoverable (linked from README)
- [ ] Examples can be copy-pasted and work
- [ ] AI assistants can read and understand the API structure